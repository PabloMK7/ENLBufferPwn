#pragma once
#include "CTRPluginFramework.hpp"



namespace CTRPluginFramework {
    class PatternManager
    {
    using   PatternCallback = bool(*)(u32);
    public:

        class PatternEntry
        {
            friend class PatternManager;
        public:
            PatternEntry(const u8* pattern, u32 size, PatternCallback cb);
            ~PatternEntry(void) = default;
        private:
            bool                isHandled;
            size_t              patsize;
            PatternCallback     callback;
            std::vector<u8>     pat;
        };

        PatternManager(void) = default;
        ~PatternManager(void) = default;

        void    Add(const u8* pattern, u32 size, PatternCallback cb);
        void    Perform(void);

    private:
        std::vector<PatternEntry> entries{};


        static s32  PerformTaskFunc(void *arg);
    };
}
