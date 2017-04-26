#include <Windows.h>

struct XUSER_SIGNIN_INFO {
	DWORD	xuidL;
	DWORD	xuidH;
	DWORD    dwInfoFlags;
	DWORD	UserSigninState;
	DWORD    dwGuestNumber;
	DWORD    dwSponsorUserIndex;
	CHAR     szUserName[16];
};

struct XUSER_READ_PROFILE_SETTINGS {
	DWORD	dwLength;
	BYTE *	pSettings;
};

struct FakeProtectedBuffer {
	DWORD	dwMagick;
	DWORD	dwSize;
	DWORD	__fill[2]; // To match buffer size in Rick's wrapper
	BYTE	bData[4];
};

void trace(char * message, ...) {}

// === Start of xlive functions ===
// #1: XWSAStartup
extern "C"  int __stdcall XWSAStartup (WORD wVersionRequested, LPWSADATA lpWsaData) { 
	lpWsaData->wVersion = 2;
	trace ("XWSAStartup \n");
	return 0;
}

// #2: XWSACleanup
extern "C"  void __stdcall XWSACleanup () {	// XWSACleanup
	trace ("XWSACleanup\n");
}

// #3: XCreateSocket
extern "C"  SOCKET __stdcall XCreateSocket (int af, int type, int protocol) { 
	trace ("XCreateSocket (%d, %d, %d)\n", af, type, protocol);
	return INVALID_SOCKET;
}

// #4: XSockeClose
extern "C"  int __stdcall XSockeClose (SOCKET s) {	
	trace ("XSockeClose)\n");
	return 0;
}

// #5: XSocketShutdown
extern "C"  int __stdcall XSocketShutdown (SOCKET s, int how) {	
	trace ("XSocketShutdown\n");
	return 0;
}

// #6: XSocketIOCTLSocket
extern "C"  int __stdcall XSocketIOCTLSocket (SOCKET s, long cmd, long * argp) {
	trace ("XSocketIOCTLSocket\n");
	return 0;
}

// #7: XSocketSetSockOpt
extern "C"  int __stdcall XSocketSetSockOpt (SOCKET s, DWORD, DWORD, DWORD, DWORD) {
	trace ("XSocketSetSockOpt\n");
	return 0;
}

// #9: XSocketGetSockName
extern "C"  int __stdcall XSocketGetSockName (SOCKET s, sockaddr_in * name, int * namelen) {
	trace ("XSocketGetSockName\n");
	if (namelen && name && *namelen == sizeof (sockaddr_in)) 
		memset (name, 0, sizeof (sockaddr_in));
	return 0;
}

// #11: XSocketBind
extern "C"  SOCKET __stdcall XSocketBind (SOCKET s, sockaddr_in * addr, int * addrlen) {
	trace ("XSocketBind\n");
	return INVALID_SOCKET;
}

// #12: XSocketConnect
extern "C"  int __stdcall XSocketConnect (SOCKET s, sockaddr_in * addr, int * addrlen) {
	trace ("XSocketConnect\n");
	return 0;
}

// #13: XSocketListen
extern "C"  int __stdcall XSocketListen (SOCKET s, int backlog) { 
	trace ("XSocketListen\n");
	return 0;
}

// #14: XSocketAccept
extern "C"  SOCKET __stdcall XSocketAccept (SOCKET s, sockaddr_in * addr, int * addrlen) { 
	trace ("XSocketAccept\n");
	return INVALID_SOCKET;
}

// #15: XSocketSelect
extern "C"  int __stdcall XSocketSelect (int n, fd_set * readfds, fd_set * writefds, fd_set * exceptfds, const struct timeval * timeout) { 
	trace ("XSocketSelect\n");
	return 0;
}

// #18: XSocketRecv
extern "C"  int __stdcall XSocketRecv (SOCKET s, char * buf, int len, int flags) { 
	return 0;
}

// #20: XSocketRecvFrom
extern "C"  int __stdcall XSocketRecvFrom (SOCKET s, char * buf, int len, int flags, sockaddr_in * from, int fromlen) { 
	return 0;
}

// #22: XSocketSend
extern "C"  int __stdcall XSocketSend (SOCKET s, char * buf, int len, int flags) {
	return 0;
}

// #24: XSocketSendTo
extern "C"  int __stdcall XSocketSendTo (SOCKET s, char * buf, int len, int flags, sockaddr_in * to, int tolen) { 
	return 0;
}

// #26: XSocketInet_Addr
extern "C"  int __stdcall XSocketInet_Addr (char *) { 
	trace ("XSocketInet_Addr\n");
	return 0;
}

// #27: XWSAGetLastError
extern "C"  int __stdcall XWSAGetLastError () {
	return WSAENETDOWN; // 0 ?
}

// #38: XSocketNTOHS
extern "C"  WORD __stdcall XSocketNTOHS (WORD n) {	
	return ((n&0xFF00) >> 8)|((n&0xFF) << 8);
}

// #39: XSocketNTOHL
extern "C"  DWORD __stdcall XSocketNTOHL (DWORD n) { 
	return ((n&0xFF000000) >> 24)|((n & 0x00FF0000) >> 8)|((n&0x0000FF00) << 8)|((n & 0x000000FF) << 24);
}

// #51: XNetStartup
extern "C"  int __stdcall XNetStartup (void *) { // XNetStartup(XNetStartupParams *)
	trace ("XNetStartup\n");
	return 0;
}

// #52: XNetCleanup
extern "C"  int __stdcall XNetCleanup () { 
	trace ("xlive_52: XNetCleanup\n");
	return 0;
}

// #54: XNetCreateKey
extern "C" int __stdcall XNetCreateKey (void * pxnkid, void * pxnkey) { 
	trace ("XNetCreateKey\n");
	return 0;
}

// #55: XNetRegisterKey
extern "C" int __stdcall XNetRegisterKey (DWORD, DWORD) { 
	return 0;
}

// #56: XNetUnregisterKey
extern "C" int __stdcall XNetUnregisterKey (DWORD) { 
	return 0;
}

// #57: XNetXnAddrToInAddr
extern "C"  int __stdcall XNetXnAddrToInAddr (DWORD, DWORD, DWORD * p) { 
	*p = 0;
	return 0;
}

// #58: XNetServerToInAddr
extern "C"  DWORD __stdcall XNetServerToInAddr (DWORD, DWORD, DWORD) { 
	return 0;
}

// #60: XNetInAddrToXnAddr
extern "C" DWORD __stdcall XNetInAddrToXnAddr (DWORD, DWORD, DWORD) { 
	return 0;
} 

// #63: XNetUnregisterInAddr
extern "C"  int __stdcall XNetUnregisterInAddr (DWORD) {
	return 0;
}

// #65: XNetConnect
extern "C" int __stdcall XNetConnect (DWORD) { 
	return 0;
}

// #66: XNetGetConnectStatus
extern "C"  int __stdcall XNetGetConnectStatus (DWORD) { 
	trace ("XNetGetConnectStatus\n");
	return 0;	
}

// #69: XNetQosListen
extern "C"  DWORD __stdcall XNetQosListen (DWORD, DWORD, DWORD, DWORD, DWORD) { 
	return 0; 
}

// #70: XNetQosLookup
extern "C"  DWORD __stdcall XNetQosLookup (DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD) { 
	return 0; 
}

// #71: XNetQosServiceLookup
extern "C" DWORD __stdcall XNetQosServiceLookup (DWORD, DWORD, DWORD) { 
	return 0;
}

// #72: XNetQosRelease
extern "C"  DWORD __stdcall XNetQosRelease (DWORD) {	
	return 0; 
}

// #73: XNetGetTitleXnAddr
extern "C"  DWORD __stdcall XNetGetTitleXnAddr (DWORD * pAddr) {
	*pAddr = 0x0100007F;	// 127.0.0.1
	return 4; 
}

// #75: XNetGetEthernetLinkStatus
extern "C"  DWORD __stdcall XNetGetEthernetLinkStatus () { 
	return 1; 
}

// #84: XNetSetSystemLinkPort
extern "C"  DWORD __stdcall XNetSetSystemLinkPort (DWORD) { 
	return 0; 
}

// #473: XCustomGetLastActionPress
extern "C" int __stdcall XCustomGetLastActionPress (DWORD, DWORD, DWORD) { 
	trace ("XCustomGetLastActionPress\n");
	return 0;
}

// #651: XNotifyGetNext
extern "C"  int __stdcall XNotifyGetNext (HANDLE hNotification, DWORD dwMsgFilter, DWORD * pdwId, void * pParam) {
	return 0;   // no notifications
}

// #652: XNotifyPositionUI
extern "C" DWORD __stdcall XNotifyPositionUI (DWORD dwPosition) {
	trace ("XNotifyPositionUI (%d)\n", dwPosition);
	return 0;
}

// #1082: XGetOverlappedExtendedError
extern "C"  DWORD __stdcall XGetOverlappedExtendedError (void *) { 
	trace ("XGetOverlappedExtendedError\n");
	return 0;
}

// #1083: XGetOverlappedResult
extern "C"  DWORD __stdcall XGetOverlappedResult (void *, DWORD * pResult, DWORD bWait) { 
	if (pResult)
		*pResult = 0;	// 0 elements enumerated
	trace ("XGetOverlappedResult\n");
	return 0;
}

// #5000: XLiveInitialize
extern "C"  int __stdcall XLiveInitialize (DWORD) {	// XLiveInitialize(struct _XLIVE_INITIALIZE_INFO *)
	trace ("XLiveInitialize\n");
	return 0;
}

// #5001: XLiveInput
extern "C"  int __stdcall XLiveInput (DWORD * p) {
	// trace ("XLiveInput\n");
	p[5] = 0;
	return 1;	// -1 ?
}


// #5002: XLiveRender
extern "C"  int __stdcall XLiveRender () {
//	trace ("XLiveRender\n");
	return 0;
}

// #5003: XLiveUninitialize
extern "C"  int __stdcall XLiveUninitialize () { 
	trace ("XLiveUninitialize\n");
	return 0;
}

// #5005: XLiveOnCreateDevice
extern "C"  int __stdcall XLiveOnCreateDevice (DWORD, DWORD) {
	trace ("XLiveOnCreateDevice\n");
	return 0;
}

// #5007: XLiveOnResetDevice
extern "C"  int __stdcall XLiveOnResetDevice (DWORD) {
	trace ("XLiveOnResetDevice\n");
	return 0;
}

// #5008: XHVCreateEngine
extern "C"  int __stdcall XHVCreateEngine (DWORD, DWORD, void ** ppEngine) { 
	trace ("XHVCreateEngine\n");
	if (ppEngine)
		*ppEngine = NULL;
	return -1;	// disable live voice   
}

// #5022: XLiveGetUpdateInformation
extern "C"  int __stdcall XLiveGetUpdateInformation (DWORD) {
	trace ("XLiveGetUpdateInformation\n");
	return -1; // no update
}

// #5024: XLiveUpdateSystem
extern "C"  int __stdcall XLiveUpdateSystem (DWORD) {
	trace ("XLiveUpdateSystem\n");
	return -1; // no update
}

// #5030: XLivePreTranslateMessage
extern "C"  int __stdcall XLivePreTranslateMessage (DWORD) {
	return 0;
}

// #5031 XLiveSetDebugLevel
extern "C" int __stdcall XLiveSetDebugLevel (DWORD xdlLevel, DWORD * pxdlOldLevel) { 
	trace ("XLiveSetDebugLevel (%d)\n", xdlLevel);
	return 0;
}

// #5214: XShowPlayerReviewUI
extern "C"  int __stdcall XShowPlayerReviewUI (DWORD, DWORD, DWORD) {
	trace ("XShowPlayerReviewUI\n");
	return 0;
}

// #5215: XShowGuideUI
extern "C"  int __stdcall XShowGuideUI (DWORD) {
	trace ("XShowGuideUI\n");
	return 1;
}

// #5216: XShowKeyboardUI
extern "C" int __stdcall XShowKeyboardUI (DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD) { 
	trace ("XShowKeyboardUI\n");
	return 0;
}

// #5251: XCloseHandle
extern "C"  int __stdcall XCloseHandle (DWORD) {
	trace ("XCloseHandle\n");
	return 0;
}

// #5252: XShowGamerCardUI
extern "C"  int __stdcall XShowGamerCardUI (DWORD, DWORD, DWORD) {
	trace ("XShowGamerCardUI\n");
	return 0;
}

// #5254: XCancelOverlapped
extern "C"  int __stdcall XCancelOverlapped (DWORD) {
	trace ("XCancelOverlapped\n");
	return 0;
}

// #5256: XEnumerate
extern "C"  int __stdcall XEnumerate (HANDLE hEnum, void * pvBuffer, DWORD cbBuffer, DWORD * pcItemsReturned, void * pOverlapped) { // XEnumerate
	trace ("XEnumerate (buffer=> %p[%d])\n", pvBuffer, cbBuffer);
//    if (pvBuffer && cbBuffer) 
//        memset (pvBuffer, 0, cbBuffer);
	if (pcItemsReturned)
		*pcItemsReturned = 0;
	return 0;	// some error ? 
}

// #5260: XShowSigninUI
extern "C"  int __stdcall XShowSigninUI (DWORD, DWORD) { 
	trace ("XShowSigninUI\n");
	return 0;
}

// #5261: XUserGetXUID
extern "C"  int __stdcall XUserGetXUID (DWORD, DWORD * pXuid) { 
	pXuid[0] = pXuid[1] = 0x10001000; 
	return 0; // ???
}


// #5262: XUserGetSigninState
extern "C"  int __stdcall XUserGetSigninState (DWORD dwUserIndex) {
//	trace ("xlive_5262: XUserGetSigninState (%d)\n", dwUserIndex);
	return 1; // eXUserSigninState_SignedInLocally
}

// #5263: XUserGetName
extern "C"  int __stdcall XUserGetName (DWORD dwUserId, char * pBuffer, DWORD dwBufLen) {
	trace ("xlive_5263: XUserGetName (%d, .. , %d)\n", dwUserId, dwBufLen);
	if (dwBufLen < 8)
		return 1;
	memcpy (pBuffer, "Player1", 8);
	return 0;
}

// #5264: XUserAreUsersFriends
extern "C"  int __stdcall XUserAreUsersFriends(DWORD dwUserIndex, DWORD * pXuids, DWORD dwXuidCount, DWORD * pResult, void * pOverlapped) {
	trace ("XUserAreUsersFriends\n");
	return ERROR_NOT_LOGGED_ON;
}

// #5265: XUserCheckPrivilege
extern "C"  int __stdcall XUserCheckPrivilege (DWORD user, DWORD priv, PBOOL b) {
	trace ("XUserCheckPrivilege (%d, %d, ..)\n", user, priv);
	*b = false;
	return ERROR_NOT_LOGGED_ON;
}



// #5267: XUserGetSigninInfo
extern "C"  int __stdcall XUserGetSigninInfo (DWORD dwUser, DWORD dwFlags, XUSER_SIGNIN_INFO * pInfo) {  
//	trace ("XUserGetSigninInfo (%d, %d, ...)\n", dwUser, dwFlags);
	pInfo->xuidL = pInfo->xuidH = dwFlags != 1 ? (dwUser+1)*0x10001000 : 0; // some arbitrary id for offline user, INVALID_XUID for online user
	if (dwFlags != 1) {
		pInfo->dwInfoFlags = 1;
		pInfo->UserSigninState = 1; // eXUserSigninState_SignedInLocally
//		strcpy (pInfo->szUserName, "Player");
	}
	return 0;
}

// #5270: XNotifyCreateListener
extern "C"  HANDLE __stdcall XNotifyCreateListener (DWORD l, DWORD h) {
	trace ("xlive_5270: XNotifyCreateListener (0x%08x%08x)\n", h, l);
	return (HANDLE)1; // any non-zero value. (zero treated as fatal error)
}

// #5273: XUserReadGamerpictureByKey
extern "C" int __stdcall XUserReadGamerpictureByKey (DWORD, DWORD, DWORD, DWORD, DWORD, DWORD) { 
	trace ("XUserReadGamerpictureByKey\n");
	return 0;
}

// #5275: XShowFriendsUI
extern "C" int __stdcall XShowFriendsUI (DWORD) {
	trace ("XShowFriendsUI\n");
	return 0;
}

// #5276: XUserSetProperty
extern "C"  int __stdcall XUserSetProperty (DWORD, DWORD, DWORD, DWORD) {
	trace ("XUserSetProperty\n");
	return 0;
}

// #5277: XUserSetContext
extern "C"  int __stdcall XUserSetContext (DWORD, DWORD, DWORD) {
	trace ("XUserSetContext\n");
	return 0;
}

// #5278: XUserWriteAchievements
extern "C"  DWORD __stdcall XUserWriteAchievements (DWORD, DWORD, DWORD) {
	trace ("XUserWriteAchievements\n");
	return 0;
}

// #5280: XUserCreateAchievementEnumerator
extern "C"  DWORD __stdcall XUserCreateAchievementEnumerator (DWORD dwTitleId, DWORD dwUserIndex, DWORD xuidL, DWORD xuidHi, DWORD dwDetailFlags, DWORD dwStartingIndex, DWORD cItem, DWORD * pcbBuffer, HANDLE * phEnum) {
	trace ("XUserCreateAchievementEnumerator (dwStartingIndex=>%d, cItem=>%d \n", dwStartingIndex, cItem);
	if (pcbBuffer)
		*pcbBuffer = 0;
	if (phEnum)
		*phEnum = INVALID_HANDLE_VALUE;
	return 1;   // return error (otherwise, 0-size buffer will be allocated)
}

// #5281: XUserReadStats
extern "C"  DWORD __stdcall XUserReadStats (DWORD, DWORD, DWORD, DWORD, DWORD, DWORD * pcbResults, DWORD * pResults, void *) { 
	trace ("XUserReadStats\n");
	if (pcbResults)	
		*pcbResults = 4;
	if (pResults)
		*pResults = 0;
	return 0;
}

// #5284: XUserCreateStatsEnumeratorByRank
extern "C"  DWORD __stdcall XUserCreateStatsEnumeratorByRank (DWORD dwTitleId, DWORD dwRankStart, DWORD dwNumRows, DWORD dwNuStatSpec, void * pSpecs, DWORD * pcbBuffer, PHANDLE phEnum) { 
	trace ("XUserCreateStatsEnumeratorByRank\n");
	if (pcbBuffer)
		*pcbBuffer = 0;
	*phEnum = INVALID_HANDLE_VALUE;
	return 1;
}

// #5286: XUserCreateStatsEnumeratorByXuid
extern "C"  DWORD __stdcall XUserCreateStatsEnumeratorByXuid (DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD * pcbBuffer, PHANDLE phEnum) { 
	trace ("XUserCreateStatsEnumeratorByXuid\n");
	if (pcbBuffer)
		pcbBuffer = 0;
	*phEnum = INVALID_HANDLE_VALUE;
	return 1;
}

// #5292: XUserSetContextEx
extern "C"  int __stdcall XUserSetContextEx (DWORD dwUserIndex, DWORD dwContextId, DWORD dwContextValue, void * pOverlapped) {
	trace ("XUserSetContextEx\n");
	return 0;
}

// #5293: XUserSetPropertyEx
extern "C" int __stdcall XUserSetPropertyEx (DWORD dwUserIndex, DWORD dwPropertyId, DWORD cbValue, void * pvValue, void * pOverlapped) { 
	trace ("XUserSetPropertyEx (%d, 0x%x, ...)\n", dwUserIndex, dwPropertyId);
	return 0;
}

// #5297: XLiveInitializeEx
extern "C" int __stdcall XLiveInitializeEx (void * pXii, DWORD dwVersion) {
	trace ("XLiveInitializeEx\n");
	return 0;
}
	
// #5300: XSessionCreate
extern "C"  DWORD __stdcall XSessionCreate (DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD) { 
	trace ("XSessionCreate\n");
	return -1;
}

// #5303: XStringVerify
extern "C"  DWORD __stdcall XStringVerify (DWORD, DWORD, DWORD, DWORD, DWORD, WORD * pResult, DWORD) { // XStringVerify
	trace ("XStringVerify\n");
	*pResult = 0;
	return 0;
}

// #5305: XStorageUploadFromMemory
extern "C"  DWORD __stdcall XStorageUploadFromMemory (DWORD, DWORD, DWORD, DWORD, DWORD) { 
	trace ("XStorageUploadFromMemory\n");
	return 0;
}

// #5306: XStorageEnumerate
extern "C" int __stdcall XStorageEnumerate (DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD) { // XStorageEnumerate
	trace ("XStorageEnumerate\n");
	return 0;
}

// #5310: XOnlineStartup
extern "C"  int __stdcall XOnlineStartup () { 
	trace ("XOnlineStartup\n");
	return 0; 
}

// #5311: XOnlineCleanup
extern "C"  int __stdcall XOnlineCleanup () {
	trace ("XOnlineCleanup\n");
	return 0;
}

// #5312: XFriendsCreateEnumerator
extern "C"  DWORD __stdcall XFriendsCreateEnumerator (DWORD, DWORD, DWORD, DWORD, HANDLE * phEnum) { 
	trace ("XFriendsCreateEnumerator\n");
	*phEnum = INVALID_HANDLE_VALUE;
	return 0; 
}

// #5314: XUserMuteListQuery
extern "C"  int __stdcall XUserMuteListQuery (DWORD, DWORD, DWORD, DWORD) { 
	trace ("XUserMuteListQuery\n");
	return 0; 
}

// #5315: XInviteGetAcceptedInfo
extern "C"  int __stdcall XInviteGetAcceptedInfo (DWORD, DWORD) { 
	trace ("XInviteGetAcceptedInfo\n");
	return 1; 
}

// #5316: XInviteSend
extern "C"  int __stdcall XInviteSend (DWORD, DWORD, DWORD, DWORD, DWORD) { 
	trace ("XInviteSend\n");
	return 0; 
}

// #5317: XSessionWriteStats
extern "C"  DWORD __stdcall XSessionWriteStats (DWORD, DWORD, DWORD, DWORD, DWORD, DWORD) { 
	trace ("XSessionWriteStats\n");
	return 0; 
}

// #5318
extern "C"  int __stdcall XSessionStart (DWORD, DWORD, DWORD) {
	trace ("XSessionStart\n");
	return 0;
}

// #5319: XSessionSearchEx
extern "C"  DWORD __stdcall XSessionSearchEx (DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD) { 
	trace ("XSessionSearchEx\n");
	return 0; 
}

// #5322: XSessionModify
extern "C"  DWORD __stdcall XSessionModify (DWORD, DWORD, DWORD, DWORD, DWORD) { 
	trace ("XSessionModify\n");
	return 0; 
}

// #5323: XSessionMigrateHost
extern "C"  DWORD __stdcall XSessionMigrateHost (DWORD, DWORD, DWORD, DWORD) {  
	trace ("XSessionMigrateHost\n");
	return 0; 
}

// #5324: XOnlineGetNatType
extern "C"  int __stdcall XOnlineGetNatType () { 
	trace ("XOnlineGetNatType\n");
	return 0; 
}

// #5325: XSessionLeaveLocal
extern "C"  DWORD __stdcall XSessionLeaveLocal (DWORD, DWORD, DWORD, DWORD) { 
	trace ("XSessionLeaveLocal\n");
	return 0; 
}

// #5326: XSessionJoinRemote
extern "C"  DWORD __stdcall XSessionJoinRemote (DWORD, DWORD, DWORD, DWORD, DWORD) { 
	trace ("XSessionJoinRemote\n");
	return 0; 
}

// #5327: XSessionJoinLocal
extern "C"  DWORD __stdcall XSessionJoinLocal (DWORD, DWORD, DWORD, DWORD, DWORD) { 
	trace ("XSessionJoinLocal\n");
	return 0; 
}

// #5328: XSessionGetDetails
extern "C"  DWORD __stdcall XSessionGetDetails (DWORD, DWORD, DWORD, DWORD) { 
	trace ("XSessionGetDetails\n");
	return 0; 
}

// #5329: XSessionFlushStats
extern "C"  int __stdcall XSessionFlushStats (DWORD, DWORD) { 
	trace ("XSessionFlushStats\n");
	return 0; 
}

// #5330: XSessionDelete
extern "C"  DWORD __stdcall XSessionDelete (DWORD, DWORD) { 
	trace ("XSessionDelete\n");
	return 0; 
}



// #5331: XUserReadProfileSettings
extern "C"  DWORD __stdcall XUserReadProfileSettings (DWORD dwTitleId, DWORD dwUserIndex, DWORD dwNumSettingIds, 
					DWORD * pdwSettingIds, DWORD * pcbResults, XUSER_READ_PROFILE_SETTINGS * pResults, DWORD pOverlapped) {
	trace ("XUserReadProfileSettings (%d, %d, %d, ..., %d, ...)\n", dwTitleId, dwUserIndex, dwNumSettingIds, *pcbResults);
	if (*pcbResults < 1036) {
		*pcbResults = 1036;	// TODO: make correct calculation by IDs.
		return ERROR_INSUFFICIENT_BUFFER;
	}
	memset (pResults, 0, *pcbResults);
	pResults->dwLength = *pcbResults-sizeof (XUSER_READ_PROFILE_SETTINGS);
	pResults->pSettings = (BYTE *)pResults+sizeof (XUSER_READ_PROFILE_SETTINGS);
	return 0;
}

// #5332: XSessionEnd
extern "C"  int __stdcall XSessionEnd (DWORD, DWORD) {	
	trace ("XSessionEnd\n");
	return 0;
}

// #5333: XSessionArbitrationRegister
extern "C"  DWORD __stdcall XSessionArbitrationRegister (DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD) { 
	trace ("XSessionArbitrationRegister\n");
	return 0; 
}

// #5335: XTitleServerCreateEnumerator
extern "C"  DWORD __stdcall XTitleServerCreateEnumerator (LPCSTR pszServerInfo, DWORD cItem, DWORD * pcbBuffer, PHANDLE phEnum) {
	trace ("XTitleServerCreateEnumerator (cItem=> %d)\n", cItem);
	*phEnum = INVALID_HANDLE_VALUE;
	return 1;
}

// #5336: XSessionLeaveRemote
extern "C"  DWORD __stdcall XSessionLeaveRemote (DWORD, DWORD, DWORD, DWORD) { 
	trace ("XSessionLeaveRemote\n");
	return 0; 
}

// #5337: XUserWriteProfileSettings
extern "C"  DWORD __stdcall XUserWriteProfileSettings (DWORD, DWORD, DWORD, DWORD) {
	trace ("XUserWriteProfileSettings\n");
	return 0;
}

// #5339: XUserReadProfileSettingsByXuid
extern "C" DWORD __stdcall XUserReadProfileSettingsByXuid (DWORD, DWORD, DWORD, DWORD,DWORD, DWORD,DWORD, DWORD,DWORD) {
	trace ("XUserReadProfileSettingsByXuid\n");
	return 0;
}

// #5343: XLiveCalculateSkill
extern "C" DWORD __stdcall XLiveCalculateSkill (DWORD, DWORD, DWORD, DWORD, DWORD) { 
	trace ("XLiveCalculateSkill\n");
	return 0;
}

// #5344: XStorageBuildServerPath
extern "C"  DWORD __stdcall XStorageBuildServerPath (DWORD dwUserIndex, DWORD StorageFacility, 
		void * pvStorageFacilityInfo, DWORD dwStorageFacilityInfoSize, 
		void * pwszItemName, void * pwszServerPath, DWORD * pdwServerPathLength) { 
	trace ("XStorageBuildServerPath\n");
	return 0; 
}

// #5345: XStorageDownloadToMemory
extern "C"  DWORD __stdcall XStorageDownloadToMemory (DWORD dwUserIndex, DWORD, DWORD, DWORD, DWORD, DWORD, void * ) { 
	trace ("XStorageDownloadToMemory\n");
	return 0; 
}

// #5349: XLiveProtectedVerifyFile
extern "C" DWORD __stdcall XLiveProtectedVerifyFile (HANDLE hContentAccess, VOID * pvReserved, PCWSTR pszFilePath) {
	trace ("XLiveProtectedVerifyFile\n");
	return 0;
}

// #5350: XLiveContentCreateAccessHandle
extern "C" DWORD __stdcall XLiveContentCreateAccessHandle (DWORD dwTitleId, void * pContentInfo, 
	DWORD dwLicenseInfoVersion, void * xebBuffer, DWORD dwOffset, HANDLE * phAccess, void * pOverlapped) {
	trace ("XLiveContentCreateAccessHandle\n");
	if (phAccess)
		*phAccess = INVALID_HANDLE_VALUE;
	return E_OUTOFMEMORY;	// TODO: fix it
}

// #5352: XLiveContentUninstall
extern "C" DWORD __stdcall XLiveContentUninstall (void * pContentInfo, void * pxuidFor, void * pInstallCallbackParams) {
	trace ("XLiveContentUninstall\n");
	return 0;
}

// #5355: XLiveContentGetPath
extern "C" DWORD __stdcall XLiveContentGetPath (DWORD dwUserIndex, void * pContentInfo, wchar_t * pszPath, DWORD * pcchPath) {
	trace ("XLiveContentGetPath\n");
	if (pcchPath)
		*pcchPath = 0;
	if (pszPath)
		*pszPath = 0;
	return 0;
}

// #5360: XLiveContentCreateEnumerator
extern "C" DWORD __stdcall XLiveContentCreateEnumerator (DWORD, void *, DWORD *pchBuffer, HANDLE * phContent) {
	trace ("XLiveContentCreateEnumerator\n");
	if (phContent)
		*phContent = INVALID_HANDLE_VALUE;
	return 0;
}

// #5361: XLiveContentRetrieveOffersByDate
extern "C" DWORD __stdcall XLiveContentRetrieveOffersByDate (DWORD dwUserIndex, DWORD dwOffserInfoVersion, 
	SYSTEMTIME * pstStartDate, void * pOffserInfoArray, DWORD * pcOfferInfo, void * pOverlapped) {
		trace ("XLiveContentRetrieveOffersByDate\n");
	if (pcOfferInfo)
		*pcOfferInfo = 0;
	return 0;
} 

// #5365: XShowMarketplaceUI
extern "C" DWORD __stdcall XShowMarketplaceUI (DWORD dwUserIndex, DWORD dwEntryPoint, ULONGLONG dwOfferId, DWORD dwContentCategories) {
	return 1;
}

// === replacements ===


// #5016: XLivePBufferAllocate
extern "C"  DWORD __stdcall XLivePBufferAllocate (int size, FakeProtectedBuffer ** pBuffer) {
//	trace ("xlive_5016: XLivePBufferAllocate (%d)\n", size);
	*pBuffer = (FakeProtectedBuffer *)malloc (size+16);
	if (!*pBuffer) {
		trace ("ERROR: XLivePBufferAllocate unable to allocate %d bytes\n", size);
		return E_OUTOFMEMORY;
	}

	(*pBuffer)->dwMagick = 0xDEADDEAD;	// some arbitrary number
	(*pBuffer)->dwSize = size;
	return 0;
}

// #5017: XLivePBufferFree
extern "C"  DWORD __stdcall XLivePBufferFree (FakeProtectedBuffer * pBuffer) {
	// trace ("xlive_5017: XLivePBufferFree\n");
	if (pBuffer && pBuffer->dwMagick == 0xDEADDEAD)
		free (pBuffer);
	return 0;
}

// #5295: XLivePBufferSetByteArray
extern "C"  DWORD __stdcall XLivePBufferSetByteArray (FakeProtectedBuffer * pBuffer, DWORD offset, BYTE * source, DWORD size) {
	if (!pBuffer || pBuffer->dwMagick != 0xDEADDEAD || !source || offset < 0 || offset+size > pBuffer->dwSize)
		return 0;
	memcpy (pBuffer->bData+offset, source, size);
	return 0;
}

// #5294: XLivePBufferGetByteArray
extern "C"  DWORD __stdcall XLivePBufferGetByteArray (FakeProtectedBuffer * pBuffer, DWORD offset, BYTE * destination, DWORD size) {
	if (!pBuffer || pBuffer->dwMagick != 0xDEADDEAD || !destination || offset < 0 || offset+size > pBuffer->dwSize)
		return 0;
	memcpy (destination, pBuffer->bData+offset, size);
	return 0;
}

// #5019: XLivePBufferSetByte
extern "C"  DWORD __stdcall XLivePBufferSetByte (FakeProtectedBuffer * pBuffer, DWORD offset, BYTE value) {
	if (!pBuffer || pBuffer->dwMagick != 0xDEADDEAD || offset < 0 || offset > pBuffer->dwSize)
		return 0;
	pBuffer->bData[offset] = value;
	return 0;
}

// #5018: XLivePBufferGetByte
extern "C"  DWORD __stdcall XLivePBufferGetByte (FakeProtectedBuffer * pBuffer, DWORD offset, BYTE * value) {
	if (!pBuffer || pBuffer->dwMagick != 0xDEADDEAD || !value || offset < 0 || offset > pBuffer->dwSize)
		return 0;
	*value = pBuffer->bData[offset];
	return 0;
}

// #5020: XLivePBufferGetDWORD
extern "C" DWORD __stdcall XLivePBufferGetDWORD (FakeProtectedBuffer * pBuffer, DWORD dwOffset, DWORD * pdwValue) {
	if (!pBuffer || pBuffer->dwMagick != 0xDEADDEAD || dwOffset < 0 || dwOffset > pBuffer->dwSize-4 || !pdwValue)
		return 0;
	*pdwValue = *(DWORD *)(pBuffer->bData+dwOffset);
	return 0;
}

// #5021: XLivePBufferSetDWORD
extern "C" DWORD __stdcall XLivePBufferSetDWORD (FakeProtectedBuffer * pBuffer, DWORD dwOffset, DWORD dwValue ) {
	if (!pBuffer || pBuffer->dwMagick != 0xDEADDEAD || dwOffset < 0 || dwOffset > pBuffer->dwSize-4)
		return 0;
	*(DWORD *)(pBuffer->bData+dwOffset) = dwValue;
	return 0;
}

// #5026: XLiveSetSponsorToken
extern "C" DWORD __stdcall XLiveSetSponsorToken (LPCWSTR pwszToken, DWORD dwTitleId) {
	trace ("XLiveSetSponsorToken (, 0x%08x)\n", dwTitleId);
	return S_OK;
}


// #5036: XLiveCreateProtectedDataContext
extern "C"  DWORD __stdcall XLiveCreateProtectedDataContext (DWORD * dwType, PHANDLE pHandle) {
	trace ("XLiveCreateProtectedDataContext\n");
	if (pHandle)
		*pHandle = (HANDLE)1;
	return 0;
}

// #5037: XLiveQueryProtectedDataInformation
extern "C"  DWORD __stdcall XLiveQueryProtectedDataInformation (HANDLE h, DWORD * p) {
	trace ("XLiveQueryProtectedDataInformation\n");
	return 0;
}

// #5038: XLiveCloseProtectedDataContext
extern "C"  DWORD __stdcall XLiveCloseProtectedDataContext (HANDLE h) {
	trace ("XLiveCloseProtectedDataContext\n");
	return 0;
}

// #5035: XLiveUnprotectData
extern "C"  DWORD __stdcall XLiveUnprotectData (BYTE * pInBuffer, DWORD dwInDataSize, BYTE * pOutBuffer, DWORD * pDataSize, HANDLE * ph) {
	trace ("XLiveUnprotectData (..., %d, ..., %d, %d)\n", dwInDataSize, *pDataSize, *(DWORD*)ph);
	if (!pDataSize || !ph)	// invalid parameter
		return E_FAIL;
	*ph = (HANDLE)1;
	if (dwInDataSize > 12)
		dwInDataSize = 12;	// RF:G workaround
	if (!pOutBuffer || *pDataSize < dwInDataSize) {
		*pDataSize = dwInDataSize;
		return ERROR_INSUFFICIENT_BUFFER;
	}
	*pDataSize = dwInDataSize;
	memcpy (pOutBuffer, pInBuffer, dwInDataSize);
	return 0;
}

// #5034: XLiveProtectData
extern "C"  DWORD __stdcall XLiveProtectData (BYTE * pInBuffer, DWORD dwInDataSize, BYTE * pOutBuffer, DWORD * pDataSize, HANDLE h) {
	trace ("XLiveProtectData (..., %d, ..., %d, %d)\n", dwInDataSize, *pDataSize, (DWORD)h);
	*pDataSize = dwInDataSize;
	if (*pDataSize >= dwInDataSize && pOutBuffer)
		memcpy (pOutBuffer, pInBuffer, dwInDataSize);
	return 0;
}

// #5367
extern "C" DWORD __stdcall xlive_5367 (HANDLE, DWORD, DWORD, BYTE *, DWORD) {
	trace  ("xlive_5367\n");
	return 1;
}

// #5372
extern "C" DWORD __stdcall xlive_5372 (HANDLE, DWORD, DWORD, DWORD, BYTE *, HANDLE) {
	trace ("xlive_5372\n");
	return 1;
}

// === end of xlive functions ===