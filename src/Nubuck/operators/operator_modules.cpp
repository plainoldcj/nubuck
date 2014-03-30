#include <operators\operators.h>

namespace OP {

void Operators::UnloadModules() {
	for(unsigned i = 0; i < _ops.size(); ++i) {
        OperatorDesc& desc = _ops[i];
		if(desc.module) {
            FreeLibrary(desc.module);
			desc.module = NULL;
		}
	}
}


static bool IsOperatorFilename(const std::basic_string<TCHAR>& filename) {
    bool prefix = 0 == filename.find(TEXT("alg_")) || 0 == filename.find(TEXT("op_")); 
    bool suffix = true;
    return prefix && suffix;
}

#ifdef UNICODE
#define std_tcout std::wcout
#else
#define std_tcout std::cout
#endif

void LoadOperators(void) {
    const std::string baseDir = common.BaseDir();
    std::basic_string<TCHAR> filename = std::basic_string<TCHAR>(baseDir.begin(), baseDir.end())  + TEXT("Operators\\*");
    WIN32_FIND_DATA ffd;
    HANDLE ff = FindFirstFile(filename.c_str(), &ffd);
    if(INVALID_HANDLE_VALUE == ff) {
        if(ERROR_FILE_NOT_FOUND == GetLastError()) {
            common.printf("WARNING - directory %s does not exist.\n", filename.c_str());
        } else {
            common.printf("ERROR - LoadOperators: FindFirstFile failed.\n");
            Crash();
        }
    }
    do {
        if(IsOperatorFilename(ffd.cFileName)) {
            std_tcout << TEXT("found operator ") << ffd.cFileName << std::endl;

            HMODULE lib = LoadLibrary(ffd.cFileName);
            if(!lib) {
                std_tcout << TEXT("ERROR: unable to load ") << ffd.cFileName << std::endl;
			}
			typedef OP::Operator* (*createOperator_t)();
            typedef OP::OperatorPanel* (*createPanel_t)();
            createOperator_t opFunc = (createOperator_t)GetProcAddress(lib, "CreateOperator");
            createPanel_t panelFunc = (createPanel_t)GetProcAddress(lib, "CreateOperatorPanel");
            if(!opFunc || !panelFunc) printf("ERROR - unable to load createoperator() function\n");
			else {
				OP::Operator* op = opFunc();
                OP::OperatorPanel* panel = panelFunc();
				OP::g_operators.Register(panel, op, lib);
			}
        }
    } while(FindNextFile(ff, &ffd));
}

} // namespace OP