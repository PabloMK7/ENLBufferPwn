#include "PatternManager.hpp"
#include "CTRPluginFramework.hpp"

namespace CTRPluginFramework {

    void PatternManager::Add(const u8* pattern, u32 size, PatternCallback cb) {
        entries.emplace_back(pattern, size, cb);
    }

    PatternManager::PatternEntry::PatternEntry(const u8* pattern, u32 size, PatternCallback cb) {
        isHandled = false;
        pat.resize(size);
        memcpy(pat.data(), pattern, size);
        callback = cb;
        patsize = size;
    }

    using PatternIter = std::vector<PatternManager::PatternEntry>::iterator;

    struct  TaskArg
    {
        PatternIter     begin;
        PatternIter     end;
    };

    s32     PatternManager::PerformTaskFunc(void *arg)
    {
        TaskArg *args = reinterpret_cast<TaskArg *>(arg);

        PatternIter iter = args->begin;
        PatternIter end = args->end;

        u32     codeStart = 0x00100000;
        u32     codeSize = Process::GetTextSize();
        u32      codeEnd = codeStart + codeSize;

        for (; iter != end; ++iter)
        {
            u32             searchBegin = codeStart;
            PatternEntry&   pattern = *iter;

            do
            {
                u32     searchSize = codeEnd - searchBegin;
                u32     addr = Utils::Search(searchBegin, searchSize, pattern.pat);

                if (addr)
                {
                    pattern.isHandled = pattern.callback(addr);
                    if (pattern.isHandled)
                        break;

                    // Re-search pattern from current addr + 1
                    searchBegin = addr + 1;
                }
                else ///< Pattern not found
                {
                    break;
                }

            } while (!pattern.isHandled && searchBegin < codeEnd);
        }

        return 0;
    }

    void    PatternManager::Perform(void)
    {
        if (!entries.size())
            return;

        
        // Split our list of entries in 2
        TaskArg  taskArgs[2];

		taskArgs[0].begin = entries.begin();
		taskArgs[0].end = std::next(entries.begin(), entries.size() / 2);
		taskArgs[1].begin = taskArgs[0].end;
		taskArgs[1].end = entries.end();

		// Use 2 Tasks to perform the hook installation
		Task    task1 = Task(PerformTaskFunc, (void *)&taskArgs[0]);
		Task    task2 = Task(PerformTaskFunc, (void *)&taskArgs[1]);

        task1.Start();
        task2.Start();

        s32 res = task1.Wait() | task2.Wait();
    }
}
