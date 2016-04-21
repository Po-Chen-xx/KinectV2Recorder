// KinectV2Recorder.h
//
// Author: Po-Chen Wu (pcwu0329@gmail.com)
//
// These codes are written mainly based on codes in Kinect for Windows SDK 2.0

//------------------------------------------------------------------------------
// <copyright file="ColorBasics.h" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

#pragma once

#include "resource.h"
#include "ImageRenderer.h"

// InfraredSourceValueMaximum is the highest value that can be returned in the InfraredFrame.
// It is cast to a float for readability in the visualization code.
#define InfraredSourceValueMaximum static_cast<float>(USHRT_MAX)

// The InfraredOutputValueMinimum value is used to set the lower limit, post processing, of the
// infrared data that we will render.
// Increasing or decreasing this value sets a brightness "wall" either closer or further away.
#define InfraredOutputValueMinimum 0.01f 

// The InfraredOutputValueMaximum value is the upper limit, post processing, of the
// infrared data that we will render.
#define InfraredOutputValueMaximum 1.0f

// The InfraredSceneValueAverage value specifies the average infrared value of the scene.
// This value was selected by analyzing the average pixel intensity for a given scene.
// Depending on the visualization requirements for a given application, this value can be
// hard coded, as was done here, or calculated by averaging the intensity for each pixel prior
// to rendering.
#define InfraredSceneValueAverage 0.08f

/// The InfraredSceneStandardDeviations value specifies the number of standard deviations
/// to apply to InfraredSceneValueAverage. This value was selected by analyzing data
/// from a given scene.
/// Depending on the visualization requirements for a given application, this value can be
/// hard coded, as was done here, or calculated at runtime.
#define InfraredSceneStandardDeviations 3.0f

class CKinectV2Recorder
{
    static const int        cMinTimestampDifferenceForFrameReSync = 30; // The minimum timestamp difference between depth and color (in ms) at which they are considered un-synchronized.
    static const int        cDepthWidth     = 512;
    static const int        cDepthHeight    = 424;
    static const int        cInfraredWidth  = 512;
    static const int        cInfraredHeight = 424;
    static const int        cColorWidth     = 1920;
    static const int        cColorHeight    = 1080;
public:
    /// <summary>
    /// Constructor
    /// </summary>
    CKinectV2Recorder();

    /// <summary>
    /// Destructor
    /// </summary>
    ~CKinectV2Recorder();

    /// <summary>
    /// Handles window messages, passes most to the class instance to handle
    /// </summary>
    /// <param name="hWnd">window message is for</param>
    /// <param name="uMsg">message</param>
    /// <param name="wParam">message data</param>
    /// <param name="lParam">additional message data</param>
    /// <returns>result of message processing</returns>
    static LRESULT CALLBACK MessageRouter(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    /// <summary>
    /// Handle windows messages for a class instance
    /// </summary>
    /// <param name="hWnd">window message is for</param>
    /// <param name="uMsg">message</param>
    /// <param name="wParam">message data</param>
    /// <param name="lParam">additional message data</param>
    /// <returns>result of message processing</returns>
    LRESULT CALLBACK        DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    /// <summary>
    /// Creates the main window and begins processing
    /// </summary>
    /// <param name="hInstance"></param>
    /// <param name="nCmdShow"></param>
    int                     Run(HINSTANCE hInstance, int nCmdShow);

private:
    HWND                    m_hWnd;
    INT64                   m_nStartTime;
    INT64                   m_nLastCounter;
    double                  m_fFreq;
    INT64                   m_nNextStatusTime;
    DWORD                   m_nFramesSinceUpdate;
    bool                    m_bRecord;
    bool                    m_bColorSynchronized;
    bool                    m_bSelect2D;
    double                  m_fFPS;

    // Current Kinect
    IKinectSensor*          m_pKinectSensor;

    // Frame reader
    IDepthFrameReader*      m_pDepthFrameReader;
    IInfraredFrameReader*   m_pInfraredFrameReader;
    IColorFrameReader*      m_pColorFrameReader;

    // Direct2D
    ID2D1Factory*           m_pD2DFactory;
    ImageRenderer*          m_pDrawDepth;
    ImageRenderer*          m_pDrawInfrared;
    ImageRenderer*          m_pDrawColor;
    RGBQUAD*                m_pDepthRGBX;
    RGBQUAD*                m_pInfraredRGBX;
    RGBQUAD*                m_pColorRGBX;

    // Index
    UINT                    m_nModel2DIndex;
    UINT                    m_nModel3DIndex;
    UINT                    m_nTypeIndex;
    UINT                    m_nLevelIndex;
    UINT                    m_nSideIndex;

    // Icon handle
    HANDLE                  m_hRecord;
    HANDLE                  m_hStop;

    // Save folder
    WCHAR                   m_cSaveFolder[MAX_PATH];

    /// <summary>
    /// Main processing function
    /// </summary>
    void                    Update();

    /// <summary>
    /// Initialize the UI controls
    /// </summary>
    void                    InitializeUIControls();

    /// <summary>
    /// Handle new UI interaction
    /// </summary>
    void                    ProcessUI(WPARAM wParam, LPARAM lParam);

    /// <summary>
    /// Initializes the default Kinect sensor
    /// </summary>
    /// <returns>S_OK on success, otherwise failure code</returns>
    HRESULT                 InitializeDefaultSensor();

    /// <summary>
    /// Handle new depth data
    /// <param name="nTime">timestamp of frame</param>
    /// <param name="pBuffer">pointer to frame data</param>
    /// <param name="nWidth">width (in pixels) of input image data</param>
    /// <param name="nHeight">height (in pixels) of input image data</param>
    /// <param name="nMinDepth">minimum reliable depth</param>
    /// <param name="nMaxDepth">maximum reliable depth</param>
    /// </summary>
    void                    ProcessDepth(INT64 nTime, const UINT16* pBuffer, int nHeight, int nWidth, USHORT nMinDepth, USHORT nMaxDepth);

    /// <summary>
    /// Handle new infrared data
    /// <param name="nTime">timestamp of frame</param>
    /// <param name="pBuffer">pointer to frame data</param>
    /// <param name="nWidth">width (in pixels) of input image data</param>
    /// <param name="nHeight">height (in pixels) of input image data</param>
    /// </summary>
    void                    ProcessInfrared(INT64 nTime, const UINT16* pBuffer, int nHeight, int nWidth);

    /// <summary>
    /// Handle new color data
    /// <param name="nTime">timestamp of frame</param>
    /// <param name="pBuffer">pointer to frame data</param>
    /// <param name="nWidth">width (in pixels) of input image data</param>
    /// <param name="nHeight">height (in pixels) of input image data</param>
    /// </summary>
    void                    ProcessColor(INT64 nTime, RGBQUAD* pBuffer, int nWidth, int nHeight);

    /// <summary>
    /// Set the status bar message
    /// </summary>
    /// <param name="szMessage">message to display</param>
    /// <param name="nShowTimeMsec">time in milliseconds for which to ignore future status messages</param>
    /// <param name="bForce">force status update</param>
    bool                    SetStatusMessage(_In_z_ WCHAR* szMessage, DWORD nShowTimeMsec, bool bForce);

    /// <summary>
    /// Save passed in image data to disk as a bitmap
    /// </summary>
    /// <param name="pBitmapBits">image data to save</param>
    /// <param name="lWidth">width (in pixels) of input image data</param>
    /// <param name="lHeight">height (in pixels) of input image data</param>
    /// <param name="wBitsPerPixel">bits per pixel of image data</param>
    /// <param name="lpszFilePath">full file path to output bitmap to</param>
    /// <returns>indicates success or failure</returns>
    HRESULT                 SaveBitmapToFile(BYTE* pBitmapBits, LONG lWidth, LONG lHeight, WORD wBitsPerPixel, LPCWSTR lpszFilePath);

    /// <summary>
    /// Check if the directory exists
    /// </summary>
    /// <param name="szDirName">directory</param>
    /// <returns>indicates exists or not</returns>
    bool                    IsDirectoryExists(WCHAR* szDirName);
};

