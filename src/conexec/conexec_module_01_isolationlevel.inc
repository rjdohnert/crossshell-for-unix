enum class IsolationLevel {
    TotalLockdown = -1,  // AppContainer 0-network, strict Job limits, ephemeral scratch disk
    EphemeralNet  = -2,  // AppContainer network allowed, transient storage wiped on exit
    PromptCommit  = -3,  // AppContainer network allowed, staged sandbox with interactive review
    TempWorkspace = -4,  // AppContainer network allowed, output saved to dedicated temp folder
    Normal        = -5   // Current user standard execution
};

