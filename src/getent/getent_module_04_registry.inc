    // =========================================================================
    // Database Registry
    // =========================================================================
    class DatabaseRegistry {
    public:
        DatabaseRegistry() {
            register_handler(std::make_unique<PasswdHandler>());
            register_handler(std::make_unique<GroupHandler>());
            register_handler(std::make_unique<HostsHandler>());
            register_handler(std::make_unique<ServicesHandler>());
            register_handler(std::make_unique<ProtocolsHandler>());
            register_handler(std::make_unique<NetworksHandler>());
        }

        void register_handler(std::unique_ptr<IDatabaseHandler> handler) {
            m_handlers[handler->name()] = std::move(handler);
        }

        IDatabaseHandler* get(const std::string& name) const {
            auto it = m_handlers.find(name);
            return (it != m_handlers.end()) ? it->second.get() : nullptr;
        }

        const std::unordered_map<std::string, std::unique_ptr<IDatabaseHandler>>& all() const {
            return m_handlers;
        }

    private:
        std::unordered_map<std::string, std::unique_ptr<IDatabaseHandler>> m_handlers;
    };

