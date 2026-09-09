#include "WaveMakerFileDialogs.h"

#include "Misc/Paths.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <commdlg.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace WaveMakerFileDialogs
{
#if PLATFORM_WINDOWS
    static bool ShowFileDialog(void* ParentWindowHandle, bool bSave, FString& OutFilename)
    {
        OutFilename.Reset();

        // Common dialogs require a writable, null-terminated filename buffer.
        TArray<WCHAR> FilenameBuffer;
        FilenameBuffer.SetNumZeroed(32768);
        FCStringWide::Strncpy(FilenameBuffer.GetData(), bSave ? L"graph_export.png" : L"file.txt", FilenameBuffer.Num());

        static const WCHAR DataFilter[] = L"MSP Data\0*.LY;*.ly\0Text Files\0*.txt;*.lst\0";
        static const WCHAR ImageFilter[] = L"PNG Image\0*.png\0";

        OPENFILENAMEW Dialog = {};
        Dialog.lStructSize = sizeof(Dialog);
        Dialog.hwndOwner = static_cast<HWND>(ParentWindowHandle);
        Dialog.lpstrFilter = bSave ? ImageFilter : DataFilter;
        Dialog.nFilterIndex = 1;
        Dialog.lpstrFile = FilenameBuffer.GetData();
        Dialog.nMaxFile = static_cast<DWORD>(FilenameBuffer.Num());
        Dialog.lpstrTitle = bSave ? L"Export Graph as Image" : L"Open Data File";
        Dialog.lpstrDefExt = bSave ? L"png" : nullptr;
        Dialog.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
        Dialog.Flags |= bSave ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST;

        const bool bAccepted = (bSave ? GetSaveFileNameW(&Dialog) : GetOpenFileNameW(&Dialog)) != 0;
        if (!bAccepted)
        {
            const DWORD ErrorCode = CommDlgExtendedError();
            if (ErrorCode != 0)
            {
                UE_LOG(LogTemp, Error, TEXT("%s dialog failed (Windows error 0x%08x)."),
                    bSave ? TEXT("Save image") : TEXT("Open data"), static_cast<uint32>(ErrorCode));
            }
            return false;
        }

        OutFilename = FilenameBuffer.GetData();
        FPaths::NormalizeFilename(OutFilename);
        return true;
    }
#endif

    bool OpenDataFile(void* ParentWindowHandle, FString& OutFilename)
    {
#if PLATFORM_WINDOWS
        return ShowFileDialog(ParentWindowHandle, false, OutFilename);
#else
        OutFilename.Reset();
        return false;
#endif
    }

    bool SaveGraphImage(void* ParentWindowHandle, FString& OutFilename)
    {
#if PLATFORM_WINDOWS
        return ShowFileDialog(ParentWindowHandle, true, OutFilename);
#else
        OutFilename.Reset();
        return false;
#endif
    }
}
