void UpdateRecentRoms(const char* filename);
void RemoveRecentRom(std::string filename);
void GetRecentRoms();
void SaveRecentRoms();
void ClearRecentRoms();
void OpenRecentROM(int listNum);

#define IDM_RECENT_RESERVED0                    65500
#define IDM_RECENT_RESERVED1                    65501

extern HMENU recentromsmenu;