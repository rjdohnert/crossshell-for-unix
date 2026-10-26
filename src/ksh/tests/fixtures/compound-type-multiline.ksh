typeset -T Address_t=(
    typeset city="Seattle"
    integer postal=98101
)
typeset -T Person_t=(
    typeset name="guest"
    Address_t address
)
Person_t employee
print "${employee.name}:${employee.address.city}:${employee.address.postal}"
