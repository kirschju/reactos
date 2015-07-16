/*
 * PROJECT:     ReactOS Spooler API
 * LICENSE:     GNU LGPL v2.1 or any later version as published by the Free Software Foundation
 * PURPOSE:     Functions related to Printers and printing
 * COPYRIGHT:   Copyright 2015 Colin Finck <colin@reactos.org>
 */

#include "precomp.h"

static DWORD
_StartDocPrinterSpooled(PSPOOLER_HANDLE pHandle, PDOC_INFO_1W pDocInfo1, PADDJOB_INFO_1W pAddJobInfo1)
{
    DWORD cbNeeded;
    DWORD dwErrorCode;
    PJOB_INFO_1W pJobInfo1 = NULL;

    // Create the spool file.
    pHandle->hSPLFile = CreateFileW(pAddJobInfo1->Path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);
    if (pHandle->hSPLFile == INVALID_HANDLE_VALUE)
    {
        dwErrorCode = GetLastError();
        ERR("CreateFileW failed for \"%S\" with error %lu!\n", pAddJobInfo1->Path, dwErrorCode);
        goto Cleanup;
    }

    // Get the size of the job information.
    GetJobW(pHandle->hPrinter, pAddJobInfo1->JobId, 1, NULL, 0, &cbNeeded);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        dwErrorCode = GetLastError();
        ERR("GetJobW failed with error %lu!\n", dwErrorCode);
        goto Cleanup;
    }

    // Allocate enough memory for the returned job information.
    pJobInfo1 = HeapAlloc(hProcessHeap, 0, cbNeeded);
    if (!pJobInfo1)
    {
        dwErrorCode = ERROR_NOT_ENOUGH_MEMORY;
        ERR("HeapAlloc failed with error %lu!\n", GetLastError());
        goto Cleanup;
    }

    // Get the job information.
    if (!GetJobW(pHandle->hPrinter, pAddJobInfo1->JobId, 1, (PBYTE)pJobInfo1, cbNeeded, &cbNeeded))
    {
        dwErrorCode = GetLastError();
        ERR("GetJobW failed with error %lu!\n", dwErrorCode);
        goto Cleanup;
    }

    // Add our document information.
    pJobInfo1->pDatatype = pDocInfo1->pDatatype;
    pJobInfo1->pDocument = pDocInfo1->pDocName;

    // Set the new job information.
    if (!SetJobW(pHandle->hPrinter, pAddJobInfo1->JobId, 1, (PBYTE)pJobInfo1, 0))
    {
        dwErrorCode = GetLastError();
        ERR("SetJobW failed with error %lu!\n", dwErrorCode);
        goto Cleanup;
    }

    // We were successful!
    pHandle->dwJobID = pAddJobInfo1->JobId;
    dwErrorCode = ERROR_SUCCESS;

Cleanup:
    if (pJobInfo1)
        HeapFree(hProcessHeap, 0, pJobInfo1);

    return dwErrorCode;
}

static DWORD
_StartDocPrinterWithRPC(PSPOOLER_HANDLE pHandle, PDOC_INFO_1W pDocInfo1)
{
    DWORD dwErrorCode;
    WINSPOOL_DOC_INFO_CONTAINER DocInfoContainer;

    DocInfoContainer.Level = 1;
    DocInfoContainer.DocInfo.pDocInfo1 = (WINSPOOL_DOC_INFO_1*)pDocInfo1;

    RpcTryExcept
    {
        dwErrorCode = _RpcStartDocPrinter(pHandle->hPrinter, &DocInfoContainer, &pHandle->dwJobID);
    }
    RpcExcept(EXCEPTION_EXECUTE_HANDLER)
    {
        dwErrorCode = RpcExceptionCode();
        ERR("_RpcStartDocPrinter failed with exception code %lu!\n", dwErrorCode);
    }
    RpcEndExcept;

    return dwErrorCode;
}

BOOL WINAPI
EnumPrintersA(DWORD Flags, LPSTR Name, DWORD Level, LPBYTE pPrinterEnum, DWORD cbBuf, LPDWORD pcbNeeded, LPDWORD pcReturned)
{
    return FALSE;
}

BOOL WINAPI
EnumPrintersW(DWORD Flags, LPWSTR Name, DWORD Level, LPBYTE pPrinterEnum, DWORD cbBuf, LPDWORD pcbNeeded, LPDWORD pcReturned)
{
    DWORD dwErrorCode;

    // Do the RPC call
    RpcTryExcept
    {
        dwErrorCode = _RpcEnumPrinters(Flags, Name, Level, pPrinterEnum, cbBuf, pcbNeeded, pcReturned);
    }
    RpcExcept(EXCEPTION_EXECUTE_HANDLER)
    {
        dwErrorCode = RpcExceptionCode();
        ERR("_RpcEnumPrinters failed with exception code %lu!\n", dwErrorCode);
    }
    RpcEndExcept;

    SetLastError(dwErrorCode);
    return (dwErrorCode == ERROR_SUCCESS);
}

BOOL WINAPI
ClosePrinter(HANDLE hPrinter)
{
    DWORD dwErrorCode;
    PSPOOLER_HANDLE pHandle = (PSPOOLER_HANDLE)hPrinter;

    // Sanity checks.
    if (!pHandle)
    {
        dwErrorCode = ERROR_INVALID_HANDLE;
        goto Cleanup;
    }

    // Do the RPC call.
    RpcTryExcept
    {
        dwErrorCode = _RpcClosePrinter(pHandle->hPrinter);
    }
    RpcExcept(EXCEPTION_EXECUTE_HANDLER)
    {
        dwErrorCode = RpcExceptionCode();
        ERR("_RpcClosePrinter failed with exception code %lu!\n", dwErrorCode);
    }
    RpcEndExcept;

    // Close any open file handle.
    if (pHandle->hSPLFile != INVALID_HANDLE_VALUE)
        CloseHandle(pHandle->hSPLFile);

    // Free the memory for the handle.
    HeapFree(hProcessHeap, 0, pHandle);

Cleanup:
    SetLastError(dwErrorCode);
    return (dwErrorCode == ERROR_SUCCESS);

}

DWORD WINAPI
DeviceCapabilitiesA(LPCSTR pDevice, LPCSTR pPort, WORD fwCapability, LPSTR pOutput, const DEVMODEA* pDevMode)
{
    return 0;
}

DWORD WINAPI
DeviceCapabilitiesW(LPCWSTR pDevice, LPCWSTR pPort, WORD fwCapability, LPWSTR pOutput, const DEVMODEW* pDevMode)
{
    return 0;
}

LONG WINAPI
DocumentPropertiesA(HWND hWnd, HANDLE hPrinter, LPSTR pDeviceName, PDEVMODEA pDevModeOutput, PDEVMODEA pDevModeInput, DWORD fMode)
{
    return 0;
}

LONG WINAPI
DocumentPropertiesW(HWND hWnd, HANDLE hPrinter, LPWSTR pDeviceName, PDEVMODEW pDevModeOutput, PDEVMODEW pDevModeInput, DWORD fMode)
{
    return 0;
}

BOOL WINAPI
EndDocPrinter(HANDLE hPrinter)
{
    DWORD dwErrorCode;
    PSPOOLER_HANDLE pHandle = (PSPOOLER_HANDLE)hPrinter;

    // Sanity checks.
    if (!pHandle)
    {
        dwErrorCode = ERROR_INVALID_HANDLE;
        goto Cleanup;
    }

    if (pHandle->hSPLFile != INVALID_HANDLE_VALUE)
    {
        // For spooled jobs, the document is finished by calling _RpcScheduleJob.
        RpcTryExcept
        {
            dwErrorCode = _RpcScheduleJob(pHandle->hPrinter, pHandle->dwJobID);
        }
        RpcExcept(EXCEPTION_EXECUTE_HANDLER)
        {
            dwErrorCode = RpcExceptionCode();
            ERR("_RpcScheduleJob failed with exception code %lu!\n", dwErrorCode);
        }
        RpcEndExcept;

        // Close the spool file handle.
        CloseHandle(pHandle->hSPLFile);
    }
    else
    {
        // In all other cases, just call _RpcEndDocPrinter.
        RpcTryExcept
        {
            dwErrorCode = _RpcEndDocPrinter(pHandle->hPrinter);
        }
        RpcExcept(EXCEPTION_EXECUTE_HANDLER)
        {
            dwErrorCode = RpcExceptionCode();
            ERR("_RpcEndDocPrinter failed with exception code %lu!\n", dwErrorCode);
        }
        RpcEndExcept;
    }

    // A new document can now be started again.
    pHandle->bStartedDoc = FALSE;

Cleanup:
    SetLastError(dwErrorCode);
    return (dwErrorCode == ERROR_SUCCESS);
}

BOOL WINAPI
EndPagePrinter(HANDLE hPrinter)
{
    DWORD dwErrorCode;
    PSPOOLER_HANDLE pHandle = (PSPOOLER_HANDLE)hPrinter;

    // Sanity checks.
    if (!pHandle)
    {
        dwErrorCode = ERROR_INVALID_HANDLE;
        goto Cleanup;
    }

    if (pHandle->hSPLFile != INVALID_HANDLE_VALUE)
    {
        // For spooled jobs, we don't need to do anything.
        dwErrorCode = ERROR_SUCCESS;
    }
    else
    {
        // In all other cases, just call _RpcEndPagePrinter.
        RpcTryExcept
        {
            dwErrorCode = _RpcEndPagePrinter(pHandle->hPrinter);
        }
        RpcExcept(EXCEPTION_EXECUTE_HANDLER)
        {
            dwErrorCode = RpcExceptionCode();
            ERR("_RpcEndPagePrinter failed with exception code %lu!\n", dwErrorCode);
        }
        RpcEndExcept;
    }

Cleanup:
    SetLastError(dwErrorCode);
    return (dwErrorCode == ERROR_SUCCESS);
}

BOOL WINAPI
GetDefaultPrinterA(LPSTR pszBuffer, LPDWORD pcchBuffer)
{
    return FALSE;
}

BOOL WINAPI
GetDefaultPrinterW(LPWSTR pszBuffer, LPDWORD pcchBuffer)
{
    return FALSE;
}

BOOL WINAPI
GetPrinterA(HANDLE hPrinter, DWORD Level, LPBYTE pPrinter, DWORD cbBuf, LPDWORD pcbNeeded)
{
    return FALSE;
}

BOOL WINAPI
GetPrinterDriverA(HANDLE hPrinter, LPSTR pEnvironment, DWORD Level, LPBYTE pDriverInfo, DWORD cbBuf, LPDWORD pcbNeeded)
{
    return FALSE;
}

BOOL WINAPI
GetPrinterDriverW(HANDLE hPrinter, LPWSTR pEnvironment, DWORD Level, LPBYTE pDriverInfo, DWORD cbBuf, LPDWORD pcbNeeded)
{
    return FALSE;
}

BOOL WINAPI
GetPrinterW(HANDLE hPrinter, DWORD Level, LPBYTE pPrinter, DWORD cbBuf, LPDWORD pcbNeeded)
{
    return FALSE;
}

BOOL WINAPI
OpenPrinterA(LPSTR pPrinterName, LPHANDLE phPrinter, LPPRINTER_DEFAULTSA pDefault)
{
    BOOL bReturnValue = FALSE;
    PWSTR pwszPrinterName = NULL;
    PWSTR pwszDatatype = NULL;
    PRINTER_DEFAULTSW wDefault = { 0 };
    size_t StringLength;

    if (pPrinterName)
    {
        // Convert pPrinterName to a Unicode string pwszPrinterName
        StringLength = strlen(pPrinterName) + 1;

        pwszPrinterName = HeapAlloc(hProcessHeap, 0, StringLength * sizeof(WCHAR));
        if (!pwszPrinterName)
        {
            ERR("HeapAlloc failed for pwszPrinterName with last error %lu!\n", GetLastError());
            goto Cleanup;
        }

        MultiByteToWideChar(CP_ACP, 0, pPrinterName, -1, pwszPrinterName, StringLength);
    }

    if (pDefault)
    {
        wDefault.DesiredAccess = pDefault->DesiredAccess;

        if (pDefault->pDatatype)
        {
            // Convert pDefault->pDatatype to a Unicode string pwszDatatype that later becomes wDefault.pDatatype
            StringLength = strlen(pDefault->pDatatype) + 1;

            pwszDatatype = HeapAlloc(hProcessHeap, 0, StringLength * sizeof(WCHAR));
            if (!pwszDatatype)
            {
                ERR("HeapAlloc failed for pwszDatatype with last error %lu!\n", GetLastError());
                goto Cleanup;
            }

            MultiByteToWideChar(CP_ACP, 0, pDefault->pDatatype, -1, pwszDatatype, StringLength);
            wDefault.pDatatype = pwszDatatype;
        }

        if (pDefault->pDevMode)
            wDefault.pDevMode = GdiConvertToDevmodeW(pDefault->pDevMode);
    }

    bReturnValue = OpenPrinterW(pwszPrinterName, phPrinter, &wDefault);

Cleanup:
    if (wDefault.pDevMode)
        HeapFree(hProcessHeap, 0, wDefault.pDevMode);

    if (pwszPrinterName)
        HeapFree(hProcessHeap, 0, pwszPrinterName);

    if (pwszDatatype)
        HeapFree(hProcessHeap, 0, pwszDatatype);

    return bReturnValue;
}

BOOL WINAPI
OpenPrinterW(LPWSTR pPrinterName, LPHANDLE phPrinter, LPPRINTER_DEFAULTSW pDefault)
{
    DWORD dwErrorCode;
    HANDLE hPrinter;
    PSPOOLER_HANDLE pHandle;
    PWSTR pDatatype = NULL;
    WINSPOOL_DEVMODE_CONTAINER DevModeContainer;
    WINSPOOL_DEVMODE_CONTAINER* pDevModeContainer = NULL;
    ACCESS_MASK AccessRequired = 0;

    // Prepare the additional parameters in the format required by _RpcOpenPrinter
    if (pDefault)
    {
        pDatatype = pDefault->pDatatype;
        DevModeContainer.cbBuf = sizeof(DEVMODEW);
        DevModeContainer.pDevMode = (BYTE*)pDefault->pDevMode;
        pDevModeContainer = &DevModeContainer;
        AccessRequired = pDefault->DesiredAccess;
    }

    // Do the RPC call
    RpcTryExcept
    {
        dwErrorCode = _RpcOpenPrinter(pPrinterName, &hPrinter, pDatatype, pDevModeContainer, AccessRequired);
    }
    RpcExcept(EXCEPTION_EXECUTE_HANDLER)
    {
        dwErrorCode = RpcExceptionCode();
        ERR("_RpcOpenPrinter failed with exception code %lu!\n", dwErrorCode);
    }
    RpcEndExcept;

    if (dwErrorCode == ERROR_SUCCESS)
    {
        // Create a new SPOOLER_HANDLE structure.
        pHandle = HeapAlloc(hProcessHeap, HEAP_ZERO_MEMORY, sizeof(SPOOLER_HANDLE));
        if (!pHandle)
        {
            dwErrorCode = ERROR_NOT_ENOUGH_MEMORY;
            ERR("HeapAlloc failed with error %lu!\n", GetLastError());
            goto Cleanup;
        }

        pHandle->hPrinter = hPrinter;
        pHandle->hSPLFile = INVALID_HANDLE_VALUE;

        // Return it as phPrinter.
        *phPrinter = (HANDLE)pHandle;
    }

Cleanup:
    SetLastError(dwErrorCode);
    return (dwErrorCode == ERROR_SUCCESS);
}

BOOL WINAPI
ReadPrinter(HANDLE hPrinter, PVOID pBuf, DWORD cbBuf, PDWORD pNoBytesRead)
{
    DWORD dwErrorCode;
    PSPOOLER_HANDLE pHandle = (PSPOOLER_HANDLE)hPrinter;

    // Sanity checks.
    if (!pHandle)
    {
        dwErrorCode = ERROR_INVALID_HANDLE;
        goto Cleanup;
    }

    // Do the RPC call
    RpcTryExcept
    {
        dwErrorCode = _RpcReadPrinter(pHandle->hPrinter, pBuf, cbBuf, pNoBytesRead);
    }
    RpcExcept(EXCEPTION_EXECUTE_HANDLER)
    {
        dwErrorCode = RpcExceptionCode();
        ERR("_RpcReadPrinter failed with exception code %lu!\n", dwErrorCode);
    }
    RpcEndExcept;

Cleanup:
    SetLastError(dwErrorCode);
    return (dwErrorCode == ERROR_SUCCESS);
}

DWORD WINAPI
StartDocPrinterW(HANDLE hPrinter, DWORD Level, PBYTE pDocInfo)
{
    DWORD cbAddJobInfo1;
    DWORD cbNeeded;
    DWORD dwErrorCode;
    PADDJOB_INFO_1W pAddJobInfo1 = NULL;
    PDOC_INFO_1W pDocInfo1 = (PDOC_INFO_1W)pDocInfo;
    PSPOOLER_HANDLE pHandle = (PSPOOLER_HANDLE)hPrinter;

    // Sanity checks.
    if (!pHandle)
    {
        dwErrorCode = ERROR_INVALID_HANDLE;
        goto Cleanup;
    }

    if (!pDocInfo1)
    {
        dwErrorCode = ERROR_INVALID_PARAMETER;
        goto Cleanup;
    }

    if (Level != 1)
    {
        dwErrorCode = ERROR_INVALID_LEVEL;
        goto Cleanup;
    }

    if (pHandle->bStartedDoc)
    {
        dwErrorCode = ERROR_INVALID_PRINTER_STATE;
        goto Cleanup;
    }

    // Check if we want to redirect output into a file.
    if (pDocInfo1->pOutputFile)
    {
        // Do a StartDocPrinter RPC call in this case.
        dwErrorCode = _StartDocPrinterWithRPC(pHandle, pDocInfo1);
    }
    else
    {
        // Allocate memory for the ADDJOB_INFO_1W structure and a path.
        cbAddJobInfo1 = sizeof(ADDJOB_INFO_1W) + MAX_PATH * sizeof(WCHAR);
        pAddJobInfo1 = HeapAlloc(hProcessHeap, 0, cbAddJobInfo1);
        if (!pAddJobInfo1)
        {
            dwErrorCode = ERROR_NOT_ENOUGH_MEMORY;
            ERR("HeapAlloc failed with error %lu!\n", GetLastError());
            goto Cleanup;
        }

        // Try to add a new job.
        // This only succeeds if the printer is set to do spooled printing.
        if (AddJobW(pHandle->hPrinter, 1, (PBYTE)pAddJobInfo1, cbAddJobInfo1, &cbNeeded))
        {
            // Do spooled printing.
            dwErrorCode = _StartDocPrinterSpooled(pHandle, pDocInfo1, pAddJobInfo1);
        }
        else if (GetLastError() == ERROR_INVALID_ACCESS)
        {
            // ERROR_INVALID_ACCESS is returned when the printer is set to do direct printing.
            // In this case, we do a StartDocPrinter RPC call.
            dwErrorCode = _StartDocPrinterWithRPC(pHandle, pDocInfo1);
        }
        else
        {
            dwErrorCode = GetLastError();
            ERR("AddJobW failed with error %lu!\n", dwErrorCode);
            goto Cleanup;
        }
    }

    if (dwErrorCode == ERROR_SUCCESS)
        pHandle->bStartedDoc = TRUE;

Cleanup:
    if (pAddJobInfo1)
        HeapFree(hProcessHeap, 0, pAddJobInfo1);

    SetLastError(dwErrorCode);
    return pHandle->dwJobID;
}

BOOL WINAPI
StartPagePrinter(HANDLE hPrinter)
{
    DWORD dwErrorCode;
    PSPOOLER_HANDLE pHandle = (PSPOOLER_HANDLE)hPrinter;

    // Sanity checks.
    if (!pHandle)
    {
        dwErrorCode = ERROR_INVALID_HANDLE;
        goto Cleanup;
    }

    // Do the RPC call
    RpcTryExcept
    {
        dwErrorCode = _RpcStartPagePrinter(pHandle->hPrinter);
    }
    RpcExcept(EXCEPTION_EXECUTE_HANDLER)
    {
        dwErrorCode = RpcExceptionCode();
        ERR("_RpcStartPagePrinter failed with exception code %lu!\n", dwErrorCode);
    }
    RpcEndExcept;

Cleanup:
    SetLastError(dwErrorCode);
    return (dwErrorCode == ERROR_SUCCESS);
}

BOOL WINAPI
WritePrinter(HANDLE hPrinter, PVOID pBuf, DWORD cbBuf, PDWORD pcWritten)
{
    DWORD dwErrorCode;
    PSPOOLER_HANDLE pHandle = (PSPOOLER_HANDLE)hPrinter;

    // Sanity checks.
    if (!pHandle)
    {
        dwErrorCode = ERROR_INVALID_HANDLE;
        goto Cleanup;
    }

    if (!pHandle->bStartedDoc)
    {
        dwErrorCode = ERROR_SPL_NO_STARTDOC;
        goto Cleanup;
    }

    if (pHandle->hSPLFile != INVALID_HANDLE_VALUE)
    {
        // Write to the spool file. This doesn't need an RPC request.
        if (!WriteFile(pHandle->hSPLFile, pBuf, cbBuf, pcWritten, NULL))
        {
            dwErrorCode = GetLastError();
            ERR("WriteFile failed with error %lu!\n", dwErrorCode);
            goto Cleanup;
        }

        dwErrorCode = ERROR_SUCCESS;
    }
    else
    {
        // TODO: This case (for direct printing or remote printing) has bad performance if multiple small-sized WritePrinter calls are performed.
        // We may increase performance by writing into a buffer and only doing a single RPC call when the buffer is full.

        // Do the RPC call
        RpcTryExcept
        {
            dwErrorCode = _RpcWritePrinter(pHandle->hPrinter, pBuf, cbBuf, pcWritten);
        }
        RpcExcept(EXCEPTION_EXECUTE_HANDLER)
        {
            dwErrorCode = RpcExceptionCode();
            ERR("_RpcWritePrinter failed with exception code %lu!\n", dwErrorCode);
        }
        RpcEndExcept;
    }

Cleanup:
    SetLastError(dwErrorCode);
    return (dwErrorCode == ERROR_SUCCESS);
}

BOOL WINAPI
XcvDataW(HANDLE hXcv, PCWSTR pszDataName, PBYTE pInputData, DWORD cbInputData, PBYTE pOutputData, DWORD cbOutputData, PDWORD pcbOutputNeeded, PDWORD pdwStatus)
{
    return FALSE;
}
