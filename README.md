# XBOX_Controller

## Main goal: 
  Use XBOX360 controller to improve control experience of PIONEER, which's a robot in my lab and recently controlled by           computers keyboards. Since there're just 4 directions on the keyboard, we can enjoy exquisiter control using XBOX               controller. Furthermore, some additional novel functionalities are available with XBOX controller.

## Completed things:
  1. Register a window class.
  2. Create a WIN32 windows.
  3. Message controlling & dispatching.
  4. Get controller's data from XInputGetState().
  5. Buttons.
  6. Combobox(resposible for font-chooser).

## TODO list:
  1. Output message & format.
  2. How to detect "Rotate".
  3. (Vibration feedback).

**If you want to get whole project, just follow these steps:**
_step1_
__make sure you have some necessary things installed:__
  1. [Visual Studio](https://www.visualstudio.com/en-us/downloads/visual-studio-2015-downloads-vs)(2010 or later recommended)
  2. [Windows SDK](https://www.microsoft.com/en-us/download/details.aspx?id=8279)(Please install the latest version)
  3. You need a XBOX controller(of course :p, or you may need this project)

_step2_
```git
git clone https://github.com/brianhuang1019/XBOX_Controller.git XBOX
```
_step3_
```
cd XBOX/XBOX_Controller/C++/SimpleController
```
_step4_
open SimpleController_2012.vcxproj then enter "F5" to build.
  
  
###### This project is built by visual studio 2012 in C++ by Po-Chih Huang.
