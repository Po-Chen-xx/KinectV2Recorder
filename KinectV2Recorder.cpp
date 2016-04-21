// KinectV2Recorder.cpp
//
// Author: Po-Chen Wu (pcwu0329@gmail.com)
//
// These codes are written mainly based on codes in Kinect for Windows SDK 2.0

//------------------------------------------------------------------------------
// <copyright file="ColorBasics.cpp" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

#include "stdafx.h"
#include <strsafe.h>
#include "resource.h"
#include "KinectV2Recorder.h"
# include   < shlwapi.h >

/// <summary>
/// Entry point for the application
/// </summary>
/// <param name="hInstance">handle to the application instance</param>
/// <param name="hPrevInstance">always 0</param>
/// <param name="lpCmdLine">command line arguments</param>
/// <param name="nCmdShow">whether to display minimized, maximized, or normally</param>
/// <returns>status</returns>
int APIENTRY wWinMain(    
	_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nShowCmd
    )
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    CKinectV2Recorder application;
    application.Run(hInstance, nShowCmd);
}

/// <summary>
/// Constructor
/// </summary>
CKinectV2Recorder::CKinectV2Recorder() :
    m_hWnd(NULL),
    m_nStartTime(0),
    m_nLastCounter(0),
    m_nFramesSinceUpdate(0),
    m_fFreq(0),
    m_nNextStatusTime(0LL),
    m_bRecord(false),
    m_bColorSynchronized(false),
    m_bSelect2D(true),
    m_pKinectSensor(NULL),
    m_pDepthFrameReader(NULL),
    m_pInfraredFrameReader(NULL),
    m_pColorFrameReader(NULL),
    m_pD2DFactory(NULL),
    m_pDrawDepth(NULL),
    m_pDrawInfrared(NULL),
    m_pDrawColor(NULL),
    m_pDepthRGBX(NULL),
    m_pInfraredRGBX(NULL),
    m_pColorRGBX(NULL),
    m_nModel2DIndex(0),
    m_nModel3DIndex(0),
    m_nTypeIndex(0),
    m_nLevelIndex(0),
    m_nSideIndex(0)
{
    LARGE_INTEGER qpf = {0};
    if (QueryPerformanceFrequency(&qpf))
    {
        m_fFreq = double(qpf.QuadPart);
    }
    
    // create heap storage for depth pixel data in RGBX format
    m_pDepthRGBX = new RGBQUAD[cDepthWidth * cDepthHeight];
    
    // create heap storage for infrared pixel data in RGBX format
    m_pInfraredRGBX = new RGBQUAD[cInfraredWidth * cInfraredHeight];

    // create heap storage for color pixel data in RGBX format
    m_pColorRGBX = new RGBQUAD[cColorWidth * cColorHeight];
}
  

/// <summary>
/// Destructor
/// </summary>
CKinectV2Recorder::~CKinectV2Recorder()
{
    // clean up Direct2D renderer
    if (m_pDrawDepth)
    {
        delete m_pDrawDepth;
        m_pDrawDepth = NULL;
    }
    
    if (m_pDrawInfrared)
    {
        delete m_pDrawInfrared;
        m_pDrawInfrared = NULL;
    }

    if (m_pDrawColor)
    {
        delete m_pDrawColor;
        m_pDrawColor = NULL;
    }
    
    if (m_pDepthRGBX)
    {
        delete [] m_pDepthRGBX;
        m_pDepthRGBX = NULL;
    }
    
    if (m_pInfraredRGBX)
    {
        delete [] m_pInfraredRGBX;
        m_pInfraredRGBX = NULL;
    }

    if (m_pColorRGBX)
    {
        delete[] m_pColorRGBX;
        m_pColorRGBX = NULL;
    }

    // clean up Direct2D
    SafeRelease(m_pD2DFactory);
    
    // done with depth frame reader
    SafeRelease(m_pDepthFrameReader);
    
    // done with infrared frame reader
    SafeRelease(m_pInfraredFrameReader);

    // done with color frame reader
    SafeRelease(m_pColorFrameReader);

    // close the Kinect Sensor
    if (m_pKinectSensor)
    {
        m_pKinectSensor->Close();
    }

    SafeRelease(m_pKinectSensor);
}

/// <summary>
/// Creates the main window and begins processing
/// </summary>
/// <param name="hInstance">handle to the application instance</param>
/// <param name="nCmdShow">whether to display minimized, maximized, or normally</param>
int CKinectV2Recorder::Run(HINSTANCE hInstance, int nCmdShow)
{
    MSG       msg = {0};
    WNDCLASS  wc;

    // Dialog custom window class
    ZeroMemory(&wc, sizeof(wc));
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.cbWndExtra    = DLGWINDOWEXTRA;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon         = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_APP));
    wc.lpfnWndProc   = DefDlgProcW;
    wc.lpszClassName = L"KinectV2RecorderAppDlgWndClass";

    if (!RegisterClassW(&wc))
    {
        return 0;
    }

    // Create main application window
    HWND hWndApp = CreateDialogParamW(
        NULL,
        MAKEINTRESOURCE(IDD_APP),
        NULL,
        (DLGPROC)CKinectV2Recorder::MessageRouter, 
        reinterpret_cast<LPARAM>(this));

    // Show window
    ShowWindow(hWndApp, nCmdShow);

    // Main message loop
    while (WM_QUIT != msg.message)
    {
        Update();

        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            // If a dialog message will be taken care of by the dialog proc
            if (hWndApp && IsDialogMessageW(hWndApp, &msg))
            {
                continue;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    return static_cast<int>(msg.wParam);
}

/// <summary>
/// Main processing function
/// </summary>
void CKinectV2Recorder::Update()
{
    if (!m_pDepthFrameReader | !m_pInfraredFrameReader | !m_pColorFrameReader)
    {
        return;
    }
    
    INT64 currentInfraredFrameTime = 0;
    INT64 currentDepthFrameTime = 0;
    INT64 currentColorFrameTime = 0;
    m_bColorSynchronized = false;   // assume we are not synchronized to start with

    IInfraredFrame* pInfraredFrame = NULL;
    IDepthFrame* pDepthFrame = NULL;
    IColorFrame* pColorFrame = NULL;

    
    // Get an infrared frame from Kinect
    HRESULT hrInfrared = m_pInfraredFrameReader->AcquireLatestFrame(&pInfraredFrame);
    // Get a depth frame from Kinect
    HRESULT hrDepth = m_pDepthFrameReader->AcquireLatestFrame(&pDepthFrame);
    // Get a color frame from Kinect
    HRESULT hrColor = m_pColorFrameReader->AcquireLatestFrame(&pColorFrame);

    if (SUCCEEDED(hrInfrared))
    {
        INT64 currentInfraredFrameTime = 0;
        IFrameDescription* pFrameDescription = NULL;
        int nWidth = 0;
        int nHeight = 0;
        UINT nBufferSize = 0;
        UINT16 *pBuffer = NULL;

        // Unit: 100 ns
        HRESULT hr = pInfraredFrame->get_RelativeTime(&currentInfraredFrameTime);

        if (SUCCEEDED(hr))
        {
            hr = pInfraredFrame->get_FrameDescription(&pFrameDescription);
        }

        if (SUCCEEDED(hr))
        {
            hr = pFrameDescription->get_Width(&nWidth);
        }

        if (SUCCEEDED(hr))
        {
            hr = pFrameDescription->get_Height(&nHeight);
        }

        if (SUCCEEDED(hr))
        {
            hr = pInfraredFrame->AccessUnderlyingBuffer(&nBufferSize, &pBuffer);            
        }

        if (SUCCEEDED(hr))
        {
            ProcessInfrared(currentInfraredFrameTime, pBuffer, nWidth, nHeight);
        }

        SafeRelease(pFrameDescription);
    }

    SafeRelease(pInfraredFrame);
    
        if (SUCCEEDED(hrDepth))
    {
        INT64 currentDepthFrameTime = 0;
        IFrameDescription* pFrameDescription = NULL;
        int nWidth = 0;
        int nHeight = 0;
        USHORT nDepthMinReliableDistance = 0;
        USHORT nDepthMaxDistance = 0;
        UINT nBufferSize = 0;
        UINT16 *pBuffer = NULL;

        // Unit: 100 ns
        HRESULT hr = pDepthFrame->get_RelativeTime(&currentDepthFrameTime);

        if (SUCCEEDED(hr))
        {
            hr = pDepthFrame->get_FrameDescription(&pFrameDescription);
        }

        if (SUCCEEDED(hr))
        {
            hr = pFrameDescription->get_Width(&nWidth);
        }

        if (SUCCEEDED(hr))
        {
            hr = pFrameDescription->get_Height(&nHeight);
        }

        if (SUCCEEDED(hr))
        {
            hr = pDepthFrame->get_DepthMinReliableDistance(&nDepthMinReliableDistance);
        }

        if (SUCCEEDED(hr))
        {
            hr = pDepthFrame->get_DepthMaxReliableDistance(&nDepthMaxDistance);
        }

        if (SUCCEEDED(hr))
        {
            hr = pDepthFrame->AccessUnderlyingBuffer(&nBufferSize, &pBuffer);            
        }

        if (SUCCEEDED(hr))
        {
            ProcessDepth(currentDepthFrameTime, pBuffer, nWidth, nHeight, nDepthMinReliableDistance, nDepthMaxDistance);
        }

        SafeRelease(pFrameDescription);
    }

    SafeRelease(pDepthFrame);
    
    if (SUCCEEDED(hrColor))
    {
        INT64 currentColorFrameTime = 0;
        IFrameDescription* pFrameDescription = NULL;
        int nWidth = 0;
        int nHeight = 0;
        ColorImageFormat imageFormat = ColorImageFormat_None;
        UINT nBufferSize = 0;
        RGBQUAD *pBuffer = NULL;

        // Unit: 100 ns
        HRESULT hr = pColorFrame->get_RelativeTime(&currentColorFrameTime);

        if (SUCCEEDED(hr))
        {
            hr = pColorFrame->get_FrameDescription(&pFrameDescription);
        }

        if (SUCCEEDED(hr))
        {
            hr = pFrameDescription->get_Width(&nWidth);
        }

        if (SUCCEEDED(hr))
        {
            hr = pFrameDescription->get_Height(&nHeight);
        }

        if (SUCCEEDED(hr))
        {
            hr = pColorFrame->get_RawColorImageFormat(&imageFormat);
        }

        if (SUCCEEDED(hr))
        {
            if (imageFormat == ColorImageFormat_Bgra)
            {
                hr = pColorFrame->AccessRawUnderlyingBuffer(&nBufferSize, reinterpret_cast<BYTE**>(&pBuffer));
            }
            else if (m_pColorRGBX)
            {
                pBuffer = m_pColorRGBX;
                nBufferSize = cColorWidth * cColorHeight * sizeof(RGBQUAD);
                hr = pColorFrame->CopyConvertedFrameDataToArray(nBufferSize, reinterpret_cast<BYTE*>(pBuffer), ColorImageFormat_Bgra);            
            }
            else
            {
                hr = E_FAIL;
            }
        }

        if (SUCCEEDED(hr))
        {
            ProcessColor(currentColorFrameTime, pBuffer, nWidth, nHeight);
        }

        SafeRelease(pFrameDescription);
    }

    SafeRelease(pColorFrame);
}

/// <summary>
/// Initialize the UI
/// </summary>
void CKinectV2Recorder::InitializeUIControls()
{
    const wchar_t *Models[] = {L"Wing", L"Duck", L"City", L"Beach", L"Firework", L"Maple"};
    const wchar_t *Types[] = { L"Translation", L"Zoom", L"In-plane Rotation", L"Out-of-plane Rotation",
                              L"Flashing Light", L"Moving Light", L"Free Movement" };
    const wchar_t *Levels[] = { L"1", L"2", L"3", L"4",  L"5" };
    const wchar_t *Sides[] = { L"Front", L"Left", L"Back", L"Right" };

    // Set the radio button for selection between 2D and 3D
    if (m_bSelect2D)
    {
        CheckDlgButton(m_hWnd, IDC_2D, BST_CHECKED);
    }
    else
    {
        CheckDlgButton(m_hWnd, IDC_3D, BST_CHECKED);
    }
    
    for (int i = 0; i < 6; i++)
    {
        SendDlgItemMessage(m_hWnd, IDC_MODEL_CBO, CB_ADDSTRING, 0, (LPARAM)Models[i]);
    }

    for (int i = 0; i < 7; i++)
    {
        SendDlgItemMessage(m_hWnd, IDC_TYPE_CBO, CB_ADDSTRING, 0, (LPARAM)Types[i]);
    }

    for (int i = 0; i < 5; i++)
    {
        SendDlgItemMessage(m_hWnd, IDC_LEVEL_CBO, CB_ADDSTRING, 0, (LPARAM)Levels[i]);
    }

    for (int i = 0; i < 4; i++)
    {
        SendDlgItemMessage(m_hWnd, IDC_SIDE_CBO, CB_ADDSTRING, 0, (LPARAM)Sides[i]);
    }

    // Set combo box index
    SendDlgItemMessage(m_hWnd, IDC_MODEL_CBO, CB_SETCURSEL, m_nModel2DIndex, 0);
    SendDlgItemMessage(m_hWnd, IDC_TYPE_CBO, CB_SETCURSEL, m_nTypeIndex, 0);
    SendDlgItemMessage(m_hWnd, IDC_LEVEL_CBO, CB_SETCURSEL, m_nLevelIndex, 0);
    SendDlgItemMessage(m_hWnd, IDC_SIDE_CBO, CB_SETCURSEL, m_nSideIndex, 0);

    // Disable the side text & combo box
    EnableWindow(GetDlgItem(m_hWnd, IDC_SIDE_TEXT), false);
    EnableWindow(GetDlgItem(m_hWnd, IDC_SIDE_CBO), false);

    // Set button icons
    m_hRecord = LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_RECORD), IMAGE_ICON, 128, 128, LR_DEFAULTCOLOR);
    m_hStop = LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_STOP), IMAGE_ICON, 128, 128, LR_DEFAULTCOLOR);
    SendDlgItemMessage(m_hWnd, IDC_BUTTON_RECORD, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)m_hRecord);

    // Set save folder
    StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"2D//wi_tr_1");
}

/// <summary>
/// Process the UI inputs
/// </summary>
/// <param name="wParam">message data</param>
/// <param name="lParam">additional message data</param>
void CKinectV2Recorder::ProcessUI(WPARAM wParam, LPARAM)
{
    const wchar_t *Model2D[] = { L"Wing", L"Duck", L"City", L"Beach", L"Firework", L"Maple" };
    const wchar_t *Model3D[] = { L"Soda", L"Chest", L"Ironman", L"House", L"Bike", L"Jet" };

    // Select 2D Model
    if (IDC_2D == LOWORD(wParam) && BN_CLICKED == HIWORD(wParam))
    {
        m_bSelect2D = true;
        SendDlgItemMessage(m_hWnd, IDC_MODEL_CBO, CB_RESETCONTENT, 0, 0 );
        for (int i = 0; i < 6; i++)
        {
            SendDlgItemMessage(m_hWnd, IDC_MODEL_CBO, CB_ADDSTRING, 0, (LPARAM)Model2D[i]);
        }

        // Setup combo boxes
        SendDlgItemMessage(m_hWnd, IDC_MODEL_CBO, CB_SETCURSEL, m_nModel2DIndex, 0);

        // Disable the side text & combo box
        EnableWindow(GetDlgItem(m_hWnd, IDC_SIDE_TEXT), false);
        EnableWindow(GetDlgItem(m_hWnd, IDC_SIDE_CBO), false);
    }
    // Select 3D Model
    if (IDC_3D == LOWORD(wParam) && BN_CLICKED == HIWORD(wParam))
    {
        m_bSelect2D = false;
        SendDlgItemMessage(m_hWnd, IDC_MODEL_CBO, CB_RESETCONTENT, 0, 0);
        for (int i = 0; i < 6; i++)
        {
            SendDlgItemMessage(m_hWnd, IDC_MODEL_CBO, CB_ADDSTRING, 0, (LPARAM)Model3D[i]);
        }

        // Setup combo boxes
        SendDlgItemMessage(m_hWnd, IDC_MODEL_CBO, CB_SETCURSEL, m_nModel3DIndex, 0);

        // Enable the side text & combo box
        EnableWindow(GetDlgItem(m_hWnd, IDC_SIDE_TEXT), true);
        EnableWindow(GetDlgItem(m_hWnd, IDC_SIDE_CBO), true);
    }
    // Model selection
    if (IDC_MODEL_CBO == LOWORD(wParam))
    {
        UINT index = (UINT)SendDlgItemMessage(m_hWnd, IDC_MODEL_CBO, CB_GETCURSEL, 0, 0);
        if (m_bSelect2D) m_nModel2DIndex = index;
        else m_nModel3DIndex = index;
    }
    // Motion type selection
    if (IDC_TYPE_CBO == LOWORD(wParam))
    {
        m_nTypeIndex = (UINT)SendDlgItemMessage(m_hWnd, IDC_TYPE_CBO, CB_GETCURSEL, 0, 0);
        if (m_nTypeIndex > 3)
        {
            EnableWindow(GetDlgItem(m_hWnd, IDC_LEVEL_TEXT), false);
            EnableWindow(GetDlgItem(m_hWnd, IDC_LEVEL_CBO), false);
        }
        else
        {
            EnableWindow(GetDlgItem(m_hWnd, IDC_LEVEL_TEXT), true);
            EnableWindow(GetDlgItem(m_hWnd, IDC_LEVEL_CBO), true);
        }
    }
    // Motion digradation level selection
    if (IDC_LEVEL_CBO == LOWORD(wParam))
    {
        m_nLevelIndex = (UINT)SendDlgItemMessage(m_hWnd, IDC_LEVEL_CBO, CB_GETCURSEL, 0, 0);
    }
    // Model side selection
    if (IDC_SIDE_CBO == LOWORD(wParam))
    {
        m_nSideIndex = (UINT)SendDlgItemMessage(m_hWnd, IDC_SIDE_CBO, CB_GETCURSEL, 0, 0);
    }
    // Set save folder

    if (m_bSelect2D)
    {
        switch (m_nModel2DIndex)
        {
            case 0: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"2D\\wi"); break;
            case 1: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"2D\\du"); break;
            case 2: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"2D\\ci"); break;
            case 3: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"2D\\be"); break;
            case 4: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"2D\\fi"); break;
            case 5: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"2D\\ma"); break;
        }
    }
    else{
        switch (m_nModel3DIndex)
        {
            case 0: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"3D\\so"); break;
            case 1: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"3D\\ch"); break;
            case 2: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"3D\\ir"); break;
            case 3: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"3D\\ho"); break;
            case 4: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"3D\\bi"); break;
            case 5: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"3D\\je"); break;
        }
    }
    switch (m_nTypeIndex)
    {
        case 0: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_tr", m_cSaveFolder); break;
        case 1: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_zo", m_cSaveFolder); break;
        case 2: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_ir", m_cSaveFolder); break;
        case 3: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_or", m_cSaveFolder); break;
        case 4: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_fl", m_cSaveFolder); break;
        case 5: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_ml", m_cSaveFolder); break;
        case 6: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_fm", m_cSaveFolder); break;
    }
    if (m_nTypeIndex < 4)
    {
        switch (m_nLevelIndex)
        {
            case 0: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_1", m_cSaveFolder); break;
            case 1: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_2", m_cSaveFolder); break;
            case 2: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_3", m_cSaveFolder); break;
            case 3: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_4", m_cSaveFolder); break;
            case 4: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_5", m_cSaveFolder); break;
        }
    }
    if (!m_bSelect2D)
    {
        switch (m_nSideIndex)
        {
            case 0: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_f", m_cSaveFolder); break;
            case 1: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_l", m_cSaveFolder); break;
            case 2: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_b", m_cSaveFolder); break;
            case 3: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_r", m_cSaveFolder); break;
        }
    }
    WCHAR szStatusMessage[64];
    StringCchPrintf(szStatusMessage, _countof(szStatusMessage), L" Save Folder: %s    FPS = %0.2f", m_cSaveFolder, m_fFPS);
    (SetStatusMessage(szStatusMessage, 500, true));
    // If it was for the record control and a button clicked event, save the video sequences
    if (IDC_BUTTON_RECORD == LOWORD(wParam) && BN_CLICKED == HIWORD(wParam))
    {
        if (m_bRecord)
        {
            m_bRecord = false;
            m_nStartTime = 0;
            SendDlgItemMessage(m_hWnd, IDC_BUTTON_RECORD, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)m_hRecord);
        }
        else
        {
            m_bRecord = true;
            SendDlgItemMessage(m_hWnd, IDC_BUTTON_RECORD, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)m_hStop);
        }
    }
}

/// <summary>
/// Handles window messages, passes most to the class instance to handle
/// </summary>
/// <param name="hWnd">window message is for</param>
/// <param name="uMsg">message</param>
/// <param name="wParam">message data</param>
/// <param name="lParam">additional message data</param>
/// <returns>result of message processing</returns>
LRESULT CALLBACK CKinectV2Recorder::MessageRouter(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CKinectV2Recorder* pThis = NULL;
    
    if (WM_INITDIALOG == uMsg)
    {
        pThis = reinterpret_cast<CKinectV2Recorder*>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }
    else
    {
        pThis = reinterpret_cast<CKinectV2Recorder*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (pThis)
    {
        return pThis->DlgProc(hWnd, uMsg, wParam, lParam);
    }

    return 0;
}

/// <summary>
/// Handle windows messages for the class instance
/// </summary>
/// <param name="hWnd">window message is for</param>
/// <param name="uMsg">message</param>
/// <param name="wParam">message data</param>
/// <param name="lParam">additional message data</param>
/// <returns>result of message processing</returns>
LRESULT CALLBACK CKinectV2Recorder::DlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);

    switch (message)
    {
        case WM_INITDIALOG:
        {
            // Bind application window handle
            m_hWnd = hWnd;

            InitializeUIControls();

            // Init Direct2D
            D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);

            // Create and initialize a new Direct2D image renderer (take a look at ImageRenderer.h)
            // We'll use this to draw the data we receive from the Kinect to the screen 
            m_pDrawInfrared = new ImageRenderer();
            HRESULT hr = m_pDrawInfrared->Initialize(GetDlgItem(m_hWnd, IDC_INFRAREDVIEW), m_pD2DFactory, cInfraredWidth, cInfraredHeight, cInfraredWidth * sizeof(RGBQUAD));
            if (FAILED(hr))
            {
                SetStatusMessage(L"Failed to initialize the Direct2D draw device.", 10000, true);
            }    
            
            m_pDrawDepth = new ImageRenderer();
            hr = m_pDrawDepth->Initialize(GetDlgItem(m_hWnd, IDC_DEPTHVIEW), m_pD2DFactory, cDepthWidth, cDepthHeight, cDepthWidth * sizeof(RGBQUAD));
            if (FAILED(hr))
            {
                SetStatusMessage(L"Failed to initialize the Direct2D draw device.", 10000, true);
            }
            
            m_pDrawColor = new ImageRenderer();
            hr = m_pDrawColor->Initialize(GetDlgItem(m_hWnd, IDC_COLORVIEW), m_pD2DFactory, cColorWidth, cColorHeight, cColorWidth * sizeof(RGBQUAD));
            if (FAILED(hr))
            {
                SetStatusMessage(L"Failed to initialize the Direct2D draw device.", 10000, true);
            }

            // Get and initialize the default Kinect sensor
            InitializeDefaultSensor();

            // Check if the necessary directories exist
            if (!IsDirectoryExists(L"2D"))
            {
                CreateDirectory(L"2D", NULL);
            }
            if (!IsDirectoryExists(L"3D"))
            {
                CreateDirectory(L"3D", NULL);
            }
        }
        break;

        // If the titlebar X is clicked, destroy app
        case WM_CLOSE:
            DestroyWindow(hWnd);
            break;

        case WM_DESTROY:
            // Quit the main message pump
            PostQuitMessage(0);
            break;

        // Handle button press
        case WM_COMMAND:
            ProcessUI(wParam, lParam);
            break;
    }

    return FALSE;
}

/// <summary>
/// Initializes the default Kinect sensor
/// </summary>
/// <returns>indicates success or failure</returns>
HRESULT CKinectV2Recorder::InitializeDefaultSensor()
{
    HRESULT hr;

    hr = GetDefaultKinectSensor(&m_pKinectSensor);
    if (FAILED(hr))
    {
        return hr;
    }

    if (m_pKinectSensor)
    {
        // Initialize the Kinect and get the readers
        IInfraredFrameSource* pInfraredFrameSource = NULL;
        IDepthFrameSource* pDepthFrameSource = NULL;
        IColorFrameSource* pColorFrameSource = NULL;

        hr = m_pKinectSensor->Open();

        if (SUCCEEDED(hr))
        {
            hr = m_pKinectSensor->get_InfraredFrameSource(&pInfraredFrameSource);
        }
        if (SUCCEEDED(hr))
        {
            hr = pInfraredFrameSource->OpenReader(&m_pInfraredFrameReader);
        }
        if (SUCCEEDED(hr))
        {
            hr = m_pKinectSensor->get_DepthFrameSource(&pDepthFrameSource);
        }
        if (SUCCEEDED(hr))
        {
            hr = pDepthFrameSource->OpenReader(&m_pDepthFrameReader);
        }
        if (SUCCEEDED(hr))
        {
            hr = m_pKinectSensor->get_ColorFrameSource(&pColorFrameSource);
        }
        if (SUCCEEDED(hr))
        {
            hr = pColorFrameSource->OpenReader(&m_pColorFrameReader);
        }

        SafeRelease(pInfraredFrameSource);
        SafeRelease(pDepthFrameSource);
        SafeRelease(pColorFrameSource);
    }

    if (!m_pKinectSensor || FAILED(hr))
    {
        SetStatusMessage(L"No ready Kinect found!", 10000, true);
        return E_FAIL;
    }

    return hr;
}

/// <summary>
/// Handle new infrared data
/// <param name="nTime">timestamp of frame</param>
/// <param name="pBuffer">pointer to frame data</param>
/// <param name="nWidth">width (in pixels) of input image data</param>
/// <param name="nHeight">height (in pixels) of input image data</param>
/// </summary>
void CKinectV2Recorder::ProcessInfrared(INT64 nTime, const UINT16* pBuffer, int nWidth, int nHeight)
{
    if (m_hWnd)
    {
        double fps = 0.0;

        LARGE_INTEGER qpcNow = {0};
        if (m_fFreq)
        {
            if (QueryPerformanceCounter(&qpcNow))
            {
                if (m_nLastCounter)
                {
                    m_nFramesSinceUpdate++;
                    fps = m_fFreq * m_nFramesSinceUpdate / double(qpcNow.QuadPart - m_nLastCounter);
                }
            }
        }

        WCHAR szStatusMessage[64];
        StringCchPrintf(szStatusMessage, _countof(szStatusMessage), L" Save Folder: %s    FPS = %0.2f", m_cSaveFolder, fps);

        if (SetStatusMessage(szStatusMessage, 1000, false))
        {
            m_nLastCounter = qpcNow.QuadPart;
            m_nFramesSinceUpdate = 0;
            m_fFPS = fps;
        }
    }
    
    if (m_pInfraredRGBX && pBuffer && (nWidth == cInfraredWidth) && (nHeight == cInfraredHeight))
    {
        RGBQUAD* pDest = m_pInfraredRGBX;

        // end pixel is start + width*height - 1
        const UINT16* pBufferEnd = pBuffer + (nWidth * nHeight);

        while (pBuffer < pBufferEnd)
        {
			// normalize the incoming infrared data (ushort) to a float ranging from 
			// [InfraredOutputValueMinimum, InfraredOutputValueMaximum] by
			// 1. dividing the incoming value by the source maximum value
			float intensityRatio = static_cast<float>(*pBuffer) / InfraredSourceValueMaximum;

			// 2. dividing by the (average scene value * standard deviations)
			intensityRatio /= InfraredSceneValueAverage * InfraredSceneStandardDeviations;
		
			// 3. limiting the value to InfraredOutputValueMaximum
			intensityRatio = min(InfraredOutputValueMaximum, intensityRatio);

			// 4. limiting the lower value InfraredOutputValueMinimym
			intensityRatio = max(InfraredOutputValueMinimum, intensityRatio);
	
			// 5. converting the normalized value to a byte and using the result
			// as the RGB components required by the image
			byte intensity = static_cast<byte>(intensityRatio * 255.0f); 
			pDest->rgbRed = intensity;
			pDest->rgbGreen = intensity;
			pDest->rgbBlue = intensity;

			++pDest;
            ++pBuffer;
        }

        // Draw the data with Direct2D
        m_pDrawInfrared->Draw(reinterpret_cast<BYTE*>(m_pInfraredRGBX), cInfraredWidth * cInfraredHeight * sizeof(RGBQUAD));

        if (m_bRecord)
        {
            if (!m_nStartTime)
            {
                if (IsDirectoryExists(m_cSaveFolder))
                {
                    MessageBox(NULL,
                        L"The related folder is not emtpy!\n",
                        L"Frames already existed",
                        MB_OK | MB_ICONERROR
                    );
                    m_bRecord = false;
                    SendDlgItemMessage(m_hWnd, IDC_BUTTON_RECORD, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)m_hRecord);
                    return;
                }
                m_nStartTime = nTime;
            }            
            
            if (!IsDirectoryExists(m_cSaveFolder))
            {
                CreateDirectory(m_cSaveFolder, NULL);
            }

            WCHAR szInfraredSaveFolder[MAX_PATH], szTempPath[MAX_PATH];

            StringCchPrintfW(szInfraredSaveFolder, _countof(szInfraredSaveFolder), L"%s\\ir", m_cSaveFolder);

            if (!IsDirectoryExists(szInfraredSaveFolder))
            {
                CreateDirectory(szInfraredSaveFolder, NULL);
            }

            StringCchPrintfW(szInfraredSaveFolder, _countof(szInfraredSaveFolder), L"%s\\%0.6f.bmp", szInfraredSaveFolder, (nTime - m_nStartTime) / 10000000.);
            // Write out the bitmap to disk
            HRESULT hr = SaveBitmapToFile(reinterpret_cast<BYTE*>(m_pInfraredRGBX), nWidth, nHeight, sizeof(RGBQUAD)* 8, szInfraredSaveFolder);
        }
    }
}

/// <summary>
/// Handle new depth data
/// <param name="nTime">timestamp of frame</param>
/// <param name="pBuffer">pointer to frame data</param>
/// <param name="nWidth">width (in pixels) of input image data</param>
/// <param name="nHeight">height (in pixels) of input image data</param>
/// <param name="nMinDepth">minimum reliable depth</param>
/// <param name="nMaxDepth">maximum reliable depth</param>
/// </summary>
void CKinectV2Recorder::ProcessDepth(INT64 nTime, const UINT16* pBuffer, int nWidth, int nHeight, USHORT nMinDepth, USHORT nMaxDepth)
{
    // Make sure we've received valid data
    if (m_pDepthRGBX && pBuffer && (nWidth == cDepthWidth) && (nHeight == cDepthHeight))
    {
        RGBQUAD* pRGBX = m_pDepthRGBX;

        // end pixel is start + width*height - 1
        const UINT16* pBufferEnd = pBuffer + (nWidth * nHeight);

        while (pBuffer < pBufferEnd)
        {
            USHORT depth = *pBuffer;

            // To convert to a byte, we're discarding the most-significant
            // rather than least-significant bits.
            // We're preserving detail, although the intensity will "wrap."
            // Values outside the reliable depth range are mapped to 0 (black).

            // Note: Using conditionals in this loop could degrade performance.
            // Consider using a lookup table instead when writing production code.
            BYTE intensity = static_cast<BYTE>((depth >= nMinDepth) && (depth <= nMaxDepth) ? (depth % 256) : 0);

            pRGBX->rgbRed   = intensity;
            pRGBX->rgbGreen = intensity;
            pRGBX->rgbBlue  = intensity;

            ++pRGBX;
            ++pBuffer;
        }

        // Draw the data with Direct2D
        m_pDrawDepth->Draw(reinterpret_cast<BYTE*>(m_pDepthRGBX), cDepthWidth * cDepthHeight * sizeof(RGBQUAD));

        if (m_bRecord && m_nStartTime)
        {
            WCHAR szDepthSaveFolder[MAX_PATH];

            StringCchPrintfW(szDepthSaveFolder, _countof(szDepthSaveFolder), L"%s\\depth", m_cSaveFolder);

            if (!IsDirectoryExists(szDepthSaveFolder))
            {
                CreateDirectory(szDepthSaveFolder, NULL);
            }

            StringCchPrintfW(szDepthSaveFolder, _countof(szDepthSaveFolder), L"%s\\%0.6f.bmp", szDepthSaveFolder, (nTime - m_nStartTime) / 10000000.);
            // Write out the bitmap to disk
            HRESULT hr = SaveBitmapToFile(reinterpret_cast<BYTE*>(m_pDepthRGBX), nWidth, nHeight, sizeof(RGBQUAD)* 8, szDepthSaveFolder);
        }
    }
}

/// <summary>
/// Handle new color data
/// <param name="nTime">timestamp of frame</param>
/// <param name="pBuffer">pointer to frame data</param>
/// <param name="nWidth">width (in pixels) of input image data</param>
/// <param name="nHeight">height (in pixels) of input image data</param>
/// </summary>
void CKinectV2Recorder::ProcessColor(INT64 nTime, RGBQUAD* pBuffer, int nWidth, int nHeight)
{
    // Make sure we've received valid data
    if (pBuffer && (nWidth == cColorWidth) && (nHeight == cColorHeight))
    {
        // Draw the data with Direct2D
        m_pDrawColor->Draw(reinterpret_cast<BYTE*>(pBuffer), cColorWidth * cColorHeight * sizeof(RGBQUAD));

        if (m_bRecord && m_nStartTime)
        {
            WCHAR szColorSaveFolder[MAX_PATH];

            StringCchPrintfW(szColorSaveFolder, _countof(szColorSaveFolder), L"%s\\rgb", m_cSaveFolder);

            if (!IsDirectoryExists(szColorSaveFolder))
            {
                CreateDirectory(szColorSaveFolder, NULL);
            }

            StringCchPrintfW(szColorSaveFolder, _countof(szColorSaveFolder), L"%s\\%0.6f.bmp", szColorSaveFolder, (nTime - m_nStartTime) / 10000000.);
            // Write out the bitmap to disk
            HRESULT hr = SaveBitmapToFile(reinterpret_cast<BYTE*>(m_pColorRGBX), nWidth, nHeight, sizeof(RGBQUAD)* 8, szColorSaveFolder);
        }
    }
}

/// <summary>
/// Set the status bar message
/// </summary>
/// <param name="szMessage">message to display</param>
/// <param name="showTimeMsec">time in milliseconds to ignore future status messages</param>
/// <param name="bForce">force status update</param>
bool CKinectV2Recorder::SetStatusMessage(_In_z_ WCHAR* szMessage, DWORD nShowTimeMsec, bool bForce)
{
    // Unit: 1 ms
    INT64 now = GetTickCount64();

    if (m_hWnd && (bForce || (m_nNextStatusTime <= now)))
    {
        SetDlgItemText(m_hWnd, IDC_STATUS, szMessage);
        m_nNextStatusTime = now + nShowTimeMsec;

        return true;
    }

    return false;
}

/// <summary>
/// Save passed in image data to disk as a bitmap
/// </summary>
/// <param name="pBitmapBits">image data to save</param>
/// <param name="lWidth">width (in pixels) of input image data</param>
/// <param name="lHeight">height (in pixels) of input image data</param>
/// <param name="wBitsPerPixel">bits per pixel of image data</param>
/// <param name="lpszFilePath">full file path to output bitmap to</param>
/// <returns>indicates success or failure</returns>
HRESULT CKinectV2Recorder::SaveBitmapToFile(BYTE* pBitmapBits, LONG lWidth, LONG lHeight, WORD wBitsPerPixel, LPCWSTR lpszFilePath)
{
    DWORD dwByteCount = lWidth * lHeight * (wBitsPerPixel / 8);

    BITMAPINFOHEADER bmpInfoHeader = {0};

    bmpInfoHeader.biSize        = sizeof(BITMAPINFOHEADER);  // Size of the header
    bmpInfoHeader.biBitCount    = wBitsPerPixel;             // Bit count
    bmpInfoHeader.biCompression = BI_RGB;                    // Standard RGB, no compression
    bmpInfoHeader.biWidth       = lWidth;                    // Width in pixels
    bmpInfoHeader.biHeight      = -lHeight;                  // Height in pixels, negative indicates it's stored right-side-up
    bmpInfoHeader.biPlanes      = 1;                         // Default
    bmpInfoHeader.biSizeImage   = dwByteCount;               // Image size in bytes

    BITMAPFILEHEADER bfh = {0};

    bfh.bfType    = 0x4D42;                                           // 'M''B', indicates bitmap
    bfh.bfOffBits = bmpInfoHeader.biSize + sizeof(BITMAPFILEHEADER);  // Offset to the start of pixel data
    bfh.bfSize    = bfh.bfOffBits + bmpInfoHeader.biSizeImage;        // Size of image + headers

    // Create the file on disk to write to
    HANDLE hFile = CreateFileW(lpszFilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    // Return if error opening file
    if (NULL == hFile) 
    {
        return E_ACCESSDENIED;
    }

    DWORD dwBytesWritten = 0;
    
    // Write the bitmap file header
    if (!WriteFile(hFile, &bfh, sizeof(bfh), &dwBytesWritten, NULL))
    {
        CloseHandle(hFile);
        return E_FAIL;
    }
    
    // Write the bitmap info header
    if (!WriteFile(hFile, &bmpInfoHeader, sizeof(bmpInfoHeader), &dwBytesWritten, NULL))
    {
        CloseHandle(hFile);
        return E_FAIL;
    }
    
    // Write the RGB Data
    if (!WriteFile(hFile, pBitmapBits, bmpInfoHeader.biSizeImage, &dwBytesWritten, NULL))
    {
        CloseHandle(hFile);
        return E_FAIL;
    }    

    // Close the file
    CloseHandle(hFile);
    return S_OK;
}

/// <summary>
/// Check if the directory exists
/// </summary>
/// <param name="szDirName">directory</param>
/// <returns>indicates exists or not</returns>
bool CKinectV2Recorder::IsDirectoryExists(WCHAR* szDirName) {
    DWORD attribs = ::GetFileAttributes(szDirName);
    if (attribs == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attribs & FILE_ATTRIBUTE_DIRECTORY);
}