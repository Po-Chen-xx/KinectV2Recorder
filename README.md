# KinectV2Recorder
**Authors:** [Po-Chen Wu](https://github.com/Po-Chen)

KinectV2Recorder is a Win32 program used to record the **Color**, **Depth**, and **Infrared** sequences captured from Kinect V2. These three different sequences are somehow synchronized in hardware (30 fps). The depth and infrared frames are captured in the exact same time, followed by the color frame (~ 6 ms later).

![alt tag](https://raw.githubusercontent.com/Po-Chen/KinectV2Recorder/master/image/KinectV2Recorder.png)

### Version
1.0.0

### System Requirements for Using Kinect V2

The system requirements for Kinect V2 according to [MSDN](https://msdn.microsoft.com/en-us/library/dn782036.aspx) are showed below.

##### a. Supported Operating Systems and Architectures
* Windows 8 (x64)
* Windows 8.1 (x64)
* Windows 8 Embedded Standard (x64)
* Windows 8.1 Embedded Standard (x64)
* Windows 10 (x64)

##### b. Recommended Hardware Configuration
* 64 bit (x64) processor
* 4 GB Memory (or more)
* i7 3.1 GHz (or higher)
* Built-in USB 3.0 host controller (only **Intel** or **Renesas** chipset is supported).
* DX11 capable graphics adapter
* A Kinect v2 sensor, which includes a power hub and USB cabling.
* **SSD** (for fast image storage)

##### c. Software Requirements
* Visual Studio 2012 or Visual Studio 2013 (or later)
* Kinect for Windows SDK 2.0 ([download](https://www.microsoft.com/en-us/download/details.aspx?id=44561))
* (Optional) Intel® Integrated Performance Primitives (IPP) ([download](https://software.intel.com/en-us/articles/free_ipp)) 

### Program Description
Color images are stored in bmp format (24-bit per pixel). Depth and infrared images are stored in pgm format (16-bit per pixel). D2D is used to achieve real-time display. I further use Intel IPP in regards to optimization. To enable IPP, please following the project setup below.

![alt tag](https://raw.githubusercontent.com/Po-Chen/KinectV2Recorder/master/image/UseIntelIPP.png)

![alt tag](https://raw.githubusercontent.com/Po-Chen/KinectV2Recorder/master/image/Preprocessor.png)