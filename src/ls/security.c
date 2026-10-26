#include "ls.h"

#include <aclapi.h>
#include <sddl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef struct WideTextBuilder {
    wchar_t *text;
    size_t length;
    size_t capacity;
} WideTextBuilder;

static bool WideTextBuilder_Append(WideTextBuilder *builder, const wchar_t *text) {
    size_t append_length = wcslen(text);
    size_t required = builder->length + append_length + 1;
    if (required > builder->capacity) {
        size_t capacity = builder->capacity ? builder->capacity : 128;
        while (capacity < required) capacity *= 2;
        wchar_t *resized = (wchar_t *)realloc(builder->text, capacity * sizeof(wchar_t));
        if (!resized) return false;
        builder->text = resized;
        builder->capacity = capacity;
    }
    memcpy(builder->text + builder->length, text, (append_length + 1) * sizeof(wchar_t));
    builder->length += append_length;
    return true;
}

static bool WideTextBuilder_AppendFormat(WideTextBuilder *builder, const wchar_t *format, ...) {
    va_list args;
    va_start(args, format);
    int required = _vscwprintf(format, args);
    va_end(args);
    if (required < 0) return false;

    size_t capacity = builder->length + (size_t)required + 1;
    if (capacity > builder->capacity) {
        size_t grown = builder->capacity ? builder->capacity : 128;
        while (grown < capacity) grown *= 2;
        wchar_t *resized = (wchar_t *)realloc(builder->text, grown * sizeof(wchar_t));
        if (!resized) return false;
        builder->text = resized;
        builder->capacity = grown;
    }
    va_start(args, format);
    int written = vswprintf_s(builder->text + builder->length,
                              builder->capacity - builder->length, format, args);
    va_end(args);
    if (written < 0) return false;
    builder->length += (size_t)written;
    return true;
}

static bool SecurityContext_IsPermissionAce(BYTE type) {
    switch (type) {
        case ACCESS_ALLOWED_ACE_TYPE:
        case ACCESS_DENIED_ACE_TYPE:
        case ACCESS_ALLOWED_OBJECT_ACE_TYPE:
        case ACCESS_DENIED_OBJECT_ACE_TYPE:
        case ACCESS_ALLOWED_CALLBACK_ACE_TYPE:
        case ACCESS_DENIED_CALLBACK_ACE_TYPE:
        case ACCESS_ALLOWED_CALLBACK_OBJECT_ACE_TYPE:
        case ACCESS_DENIED_CALLBACK_OBJECT_ACE_TYPE:
            return true;
        default:
            return false;
    }
}

static bool SecurityContext_IsDenyAce(BYTE type) {
    return type == ACCESS_DENIED_ACE_TYPE || type == ACCESS_DENIED_OBJECT_ACE_TYPE ||
           type == ACCESS_DENIED_CALLBACK_ACE_TYPE || type == ACCESS_DENIED_CALLBACK_OBJECT_ACE_TYPE;
}

static void SecurityContext_FormatRwx(ACCESS_MASK mask, wchar_t out[4]) {
    out[0] = ((mask & FILE_GENERIC_READ) == FILE_GENERIC_READ || (mask & FILE_READ_DATA) ||
              (mask & GENERIC_READ) || (mask & GENERIC_ALL)) ? L'r' : L'-';
    out[1] = ((mask & FILE_GENERIC_WRITE) == FILE_GENERIC_WRITE || (mask & FILE_WRITE_DATA) ||
              (mask & GENERIC_WRITE) || (mask & GENERIC_ALL)) ? L'w' : L'-';
    out[2] = ((mask & FILE_GENERIC_EXECUTE) == FILE_GENERIC_EXECUTE || (mask & FILE_EXECUTE) ||
              (mask & GENERIC_EXECUTE) || (mask & GENERIC_ALL)) ? L'x' : L'-';
    out[3] = L'\0';
}

static void SecurityContext_GetAceSid(const ACE_HEADER *ace, PSID *sid) {
    *sid = NULL;
    switch (ace->AceType) {
        case ACCESS_ALLOWED_ACE_TYPE:
        case ACCESS_DENIED_ACE_TYPE:
        case SYSTEM_AUDIT_ACE_TYPE:
        case SYSTEM_ALARM_ACE_TYPE:
        case ACCESS_ALLOWED_CALLBACK_ACE_TYPE:
        case ACCESS_DENIED_CALLBACK_ACE_TYPE:
        case SYSTEM_AUDIT_CALLBACK_ACE_TYPE:
        case SYSTEM_ALARM_CALLBACK_ACE_TYPE:
        case SYSTEM_MANDATORY_LABEL_ACE_TYPE:
        case SYSTEM_RESOURCE_ATTRIBUTE_ACE_TYPE:
        case SYSTEM_SCOPED_POLICY_ID_ACE_TYPE:
        case SYSTEM_PROCESS_TRUST_LABEL_ACE_TYPE:
        case SYSTEM_ACCESS_FILTER_ACE_TYPE:
            *sid = (PSID)&((ACCESS_ALLOWED_ACE *)ace)->SidStart;
            return;
        case ACCESS_ALLOWED_OBJECT_ACE_TYPE:
        case ACCESS_DENIED_OBJECT_ACE_TYPE:
        case SYSTEM_AUDIT_OBJECT_ACE_TYPE:
        case SYSTEM_ALARM_OBJECT_ACE_TYPE:
        case ACCESS_ALLOWED_CALLBACK_OBJECT_ACE_TYPE:
        case ACCESS_DENIED_CALLBACK_OBJECT_ACE_TYPE:
        case SYSTEM_AUDIT_CALLBACK_OBJECT_ACE_TYPE:
        case SYSTEM_ALARM_CALLBACK_OBJECT_ACE_TYPE: {
            const ACCESS_ALLOWED_OBJECT_ACE *object_ace = (const ACCESS_ALLOWED_OBJECT_ACE *)ace;
            size_t offset = offsetof(ACCESS_ALLOWED_OBJECT_ACE, ObjectType);
            if (object_ace->Flags & ACE_OBJECT_TYPE_PRESENT) offset += sizeof(GUID);
            if (object_ace->Flags & ACE_INHERITED_OBJECT_TYPE_PRESENT) offset += sizeof(GUID);
            if (offset < ace->AceSize) *sid = (PSID)((const BYTE *)ace + offset);
            return;
        }
        default:
            return;
    }
}

static void SecurityContext_ResolveSid(ISecurityContext *self, PSID sid,
                                       wchar_t *out_name, DWORD max_len) {
    if (!sid || !IsValidSid(sid)) {
        wcsncpy_s(out_name, max_len, L"UNKNOWN", _TRUNCATE);
        return;
    }
    for (size_t i = 0; i < self->cache_count; i++) {
        if (EqualSid(self->cache[i].sid, sid)) {
            wcsncpy_s(out_name, max_len, self->cache[i].name, _TRUNCATE);
            return;
        }
    }

    wchar_t account_name[256];
    wchar_t domain_name[256];
    DWORD account_len = 256;
    DWORD domain_len = 256;
    SID_NAME_USE sid_use;
    if (LookupAccountSidW(NULL, sid, account_name, &account_len,
                          domain_name, &domain_len, &sid_use)) {
        if (domain_len > 0) {
            _snwprintf_s(out_name, max_len, _TRUNCATE, L"%s\\%s", domain_name, account_name);
        } else {
            wcsncpy_s(out_name, max_len, account_name, _TRUNCATE);
        }
    } else {
        LPWSTR string_sid = NULL;
        if (ConvertSidToStringSidW(sid, &string_sid)) {
            wcsncpy_s(out_name, max_len, string_sid, _TRUNCATE);
            LocalFree(string_sid);
        } else {
            wcsncpy_s(out_name, max_len, L"UNKNOWN", _TRUNCATE);
        }
    }

    if (self->cache_count < SID_CACHE_CAPACITY) {
        DWORD sid_len = GetLengthSid(sid);
        self->cache[self->cache_count].sid = (PSID)malloc(sid_len);
        if (self->cache[self->cache_count].sid) {
            CopySid(sid_len, self->cache[self->cache_count].sid, sid);
            wcsncpy_s(self->cache[self->cache_count].name, 128, out_name, _TRUNCATE);
            self->cache_count++;
        }
    }
}

static void SecurityContext_InspectAcl(ISecurityContext *self, HANDLE hFile,
                                       wchar_t *owner, DWORD max_owner,
                                       wchar_t *group, DWORD max_group,
                                       LsDaclState *dacl_state, bool collect_acl,
                                       wchar_t **acl_entries) {
    *dacl_state = LS_DACL_UNKNOWN;
    wcsncpy_s(owner, max_owner, L"UNKNOWN", _TRUNCATE);
    wcsncpy_s(group, max_group, L"UNKNOWN", _TRUNCATE);
    PSECURITY_DESCRIPTOR security_descriptor = NULL;
    PSID owner_sid = NULL;
    PSID group_sid = NULL;
    DWORD result = GetSecurityInfo(hFile, SE_FILE_OBJECT,
                                  OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
                                      DACL_SECURITY_INFORMATION,
                                  &owner_sid, &group_sid, NULL, NULL, &security_descriptor);
    if (result != ERROR_SUCCESS) {
        if (collect_acl) {
            WideTextBuilder builder = {0};
            if (WideTextBuilder_AppendFormat(&builder, L"DACL unavailable (GetSecurityInfo error %lu)\n",
                                             (unsigned long)result)) *acl_entries = builder.text;
            else free(builder.text);
        }
        return;
    }
    if (owner_sid) self->ResolveSid(self, owner_sid, owner, max_owner);
    if (group_sid) self->ResolveSid(self, group_sid, group, max_group);

    BOOL dacl_present = FALSE;
    BOOL dacl_defaulted = FALSE;
    PACL dacl = NULL;
    BOOL descriptor_has_dacl = GetSecurityDescriptorDacl(
        security_descriptor, &dacl_present, &dacl, &dacl_defaulted);
    if (descriptor_has_dacl) {
        *dacl_state = dacl_present ? LS_DACL_PRESENT : LS_DACL_ABSENT;
    }

    if (collect_acl) {
        WideTextBuilder builder = {0};
        if (!descriptor_has_dacl) {
            WideTextBuilder_Append(&builder, L"DACL unavailable (GetSecurityDescriptorDacl failed)\n");
        } else if (!dacl_present) {
            WideTextBuilder_Append(&builder, L"DACL: absent\n");
        } else if (!dacl) {
            WideTextBuilder_Append(&builder, L"DACL: NULL (all access allowed)\n");
        } else if (dacl->AceCount == 0) {
            WideTextBuilder_Append(&builder, L"DACL: empty\n");
        } else {
            for (WORD i = 0; i < dacl->AceCount; i++) {
                PACE_HEADER ace = NULL;
                wchar_t principal[128] = L"UNKNOWN";
                wchar_t permissions[4];
                ACCESS_MASK mask = 0;
                PSID sid = NULL;
                if (!GetAce(dacl, i, (LPVOID *)&ace) || ace->AceSize < sizeof(ACE_HEADER) + sizeof(ACCESS_MASK)) {
                    WideTextBuilder_AppendFormat(&builder, L"  [%u] INVALID_ACE\n", i);
                    continue;
                }
                if (!SecurityContext_IsPermissionAce(ace->AceType)) continue;
                memcpy(&mask, (const BYTE *)ace + sizeof(ACE_HEADER), sizeof(mask));
                SecurityContext_FormatRwx(mask, permissions);
                SecurityContext_GetAceSid(ace, &sid);
                if (sid) {
                    ULONG_PTR ace_start = (ULONG_PTR)ace;
                    ULONG_PTR sid_start = (ULONG_PTR)sid;
                    if (sid_start >= ace_start && sid_start - ace_start <= ace->AceSize) {
                        size_t available = ace->AceSize - (size_t)(sid_start - ace_start);
                        if (available >= offsetof(SID, SubAuthority)) {
                            const SID *sid_data = (const SID *)sid;
                            size_t required = offsetof(SID, SubAuthority) +
                                              (size_t)sid_data->SubAuthorityCount * sizeof(DWORD);
                            if (sid_data->SubAuthorityCount <= SID_MAX_SUB_AUTHORITIES &&
                                required <= available && IsValidSid(sid) && GetLengthSid(sid) <= available) {
                                self->ResolveSid(self, sid, principal,
                                                 sizeof(principal) / sizeof(wchar_t));
                            }
                        }
                    }
                }
                WideTextBuilder_AppendFormat(&builder, L"  %s  %-3s  %s [%s]\n",
                    SecurityContext_IsDenyAce(ace->AceType) ? L"DENY" : L"ALLOW",
                    permissions, principal,
                    (ace->AceFlags & INHERITED_ACE) ? L"inherited" : L"explicit");
            }
        }
        *acl_entries = builder.text;
    }
    LocalFree(security_descriptor);
}

static void SecurityContext_Destroy(ISecurityContext *self) {
    for (size_t i = 0; i < self->cache_count; i++) free(self->cache[i].sid);
    self->cache_count = 0;
}

ISecurityContext SecurityContext_Create(void) {
    ISecurityContext context;
    memset(&context, 0, sizeof(context));
    context.ResolveSid = SecurityContext_ResolveSid;
    context.InspectAcl = SecurityContext_InspectAcl;
    context.Destroy = SecurityContext_Destroy;
    return context;
}