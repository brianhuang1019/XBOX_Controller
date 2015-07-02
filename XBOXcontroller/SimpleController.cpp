//-----------------------------------------------------------------------------
// File: SimpleController.cpp
//
// Simple read of XInput gamepad controller state
//
// Note: This sample works with all versions of XInput (1.4, 1.3, and 9.1.0)
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <commdlg.h>
#include <basetsd.h>
#include <objbase.h>
#include <shellapi.h>
#include <math.h>
#include "Aria.h"

#ifdef USE_DIRECTX_SDK
#include <C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\include\xinput.h>
#pragma comment(lib,"xinput.lib")
#elif (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/)
#include <XInput.h>
#pragma comment(lib,"xinput.lib")
#else
#include <XInput.h>
#pragma comment(lib,"xinput9_1_0.lib")
#endif


//-----------------------------------------------------------------------------
// Function-prototypes
//-----------------------------------------------------------------------------
LRESULT WINAPI MsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
HRESULT UpdateControllerState();
void RenderFrame();


//-----------------------------------------------------------------------------
// Defines, constants, and global variables
//-----------------------------------------------------------------------------
#define MAX_CONTROLLERS 4                          // XInput handles up to 4 controllers 
#define INPUT_DEADZONE  ( 0.24f * FLOAT(0x7FFF) )  // Default to 24% of the +/- 32767 range.   This is a reasonable default value but can be altered if needed.
#define BUTTON_ONE 3301
#define BUTTON_TWO 3302
#define BUTTON_THREE 3303
#define COMBOBOX1 3304
#define backgroundColor 0x14FDFD                   //REMIND: Windows' rgb is represent in reverse order!
#define textColor 0x000000

#define CONS1 (1.05/2)
#define CONS2 (1.732/2)
#define ROTATE_DELAY 18

struct CONTROLLER_STATE
{
    XINPUT_STATE state;
    bool bConnected;  //記錄搖桿有沒有連線
};

CONTROLLER_STATE g_Controllers[MAX_CONTROLLERS];
WCHAR   g_szMessage[4][1024] = {0};  //記錄四個搖桿的狀態(XInputGetState拿到的值)
HWND    g_hWnd;                      //主控台視窗控制代碼
bool    g_bDeadZoneOn = true;        //記錄deadzone開或關
HINSTANCE  hg_app;                   //記錄button的HINSTANCE
int		button_flag[3];				 //用來紀錄button有無被clicked
int     currentFont;                 //記錄font
int     rotate_flag;                 //easy rotate的開關

// Central object that is an interface to the robot and its integrated
// devices, and which manages control of the robot by the rest of the program.
ArRobot robot;

bool handleDebugMessage(ArRobotPacket *pkt)
{
  if(pkt->getID() != ArCommands::MARCDEBUG) return false;
  char msg[256];
  pkt->bufToStr(msg, sizeof(msg));
  msg[255] = 0;
  ArLog::log(ArLog::Terse, "Controller Firmware Debug: %s", msg);
  return true;
}

//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Entry point for the application.  Since we use a simple dialog for 
//       user interaction we don't need to pump messages.
//-----------------------------------------------------------------------------
int WINAPI wWinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int )
{
    // Initialize COM
    HRESULT hr;
    if( FAILED( hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED) ) )
        return 1;

    // Register the window class
    HBRUSH hBrush = CreateSolidBrush( backgroundColor );
    WNDCLASSEX wc =  //第二個參數為message的dispatch函式
    {
        sizeof( WNDCLASSEX ), 0, MsgProc, 0L, 0L, hInstance, nullptr,
        LoadCursor( nullptr, IDC_ARROW ), hBrush,
        nullptr, L"XInputSample", nullptr
    };
    RegisterClassEx( &wc );

    // Create the application's window
    g_hWnd = CreateWindow( L"XInputSample", L"Controller Dectector",
                           WS_OVERLAPPED | WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,  //有標題邊框, 可顯示, 有標題列, 標題列有控制開關, 有最小化
                           CW_USEDEFAULT, CW_USEDEFAULT, 750, 650,
                           nullptr, nullptr, hInstance, nullptr );

    // Set buttons hInstance
	for (int i = 0; i < 3; i++)
		button_flag[i] = 0;
    hg_app = hInstance;

    /* include ARIA codes */

    // Initialize some global data
     Aria::init();
    
    // If you want ArLog to print "Verbose" level messages uncomment this:
    //ArLog::init(ArLog::StdOut, ArLog::Verbose);

    //get command line argument + 轉型成char**
    int argc;
    LPWSTR *tmpargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (NULL == tmpargv) {
        wprintf(L"CommandLineToArgvW failed\n");
    }
    char **argv = (char**)malloc(sizeof(char*) * argc);
    for (int i = 0; i < argc; i++) {
        argv[i] = (char*)malloc(sizeof(char) * 500);
        wcstombs(argv[i], tmpargv[i], 500);
    }
    
    //Free memory allocated for CommandLineToArgvW arguments.
    //LocalFree(argv);
    
    // This object parses program options from the command line
     ArArgumentParser parser(&argc, argv);
    
    // Load some default values for command line arguments from /etc/Aria.args
    // (Linux) or the ARIAARGS environment variable.
     parser.loadDefaultArguments();
    
    // Object that connects to the robot or simulator using program options
     ArRobotConnector robotConnector(&parser, &robot);
    
    // If the robot has an Analog Gyro, this object will activate it, and 
    // if the robot does not automatically use the gyro to correct heading,
    // this object reads data from it and corrects the pose in ArRobot
     ArAnalogGyro gyro(&robot);
    
    robot.addPacketHandler(new ArGlobalRetFunctor1<bool, ArRobotPacket*>(&handleDebugMessage));
    
    // Connect to the robot, get some initial data from it such as type and name,
    // and then load parameter files for this robot.
     if (!robotConnector.connectRobot())
     {
       // Error connecting:
       // if the user gave the -help argumentp, then just print out what happened,
       // and continue so options can be displayed later.
       if (!parser.checkHelpAndWarnUnparsed())
       {
         ArLog::log(ArLog::Terse, "Could not connect to robot, will not have parameter file so options displayed later may not include everything");
       }
       // otherwise abort
       else
       {
         ArLog::log(ArLog::Terse, "Error, could not connect to robot.");
         Aria::logOptions();
         Aria::exit(1);
       }
     }
    
    if(!robot.isConnected())
     {
       ArLog::log(ArLog::Terse, "Internal error: robot connector succeeded but ArRobot::isConnected() is false!");
     }
    
    // Connector for laser rangefinders
     ArLaserConnector laserConnector(&parser, &robot, &robotConnector);
    
    // Connector for compasses
     ArCompassConnector compassConnector(&parser);
    
    // Parse the command line options. Fail and print the help message if the parsing fails
    // or if the help was requested with the -help option
     if (!Aria::parseArgs() || !parser.checkHelpAndWarnUnparsed())
     {    
       Aria::logOptions();
       Aria::exit(1);
       return 1;
     }
    
    // Used to access and process sonar range data
     ArSonarDevice sonarDev;
     
    // Used to perform actions when keyboard keys are pressed
     ArKeyHandler keyHandler;
     Aria::setKeyHandler(&keyHandler);
    
    // ArRobot contains an exit action for the Escape key. It also 
    // stores a pointer to the keyhandler so that other parts of the program can
    // use the same keyhandler.
     robot.attachKeyHandler(&keyHandler);
     printf("You may press escape to exit\n");
    
    // Attach sonarDev to the robot so it gets data from it.
     robot.addRangeDevice(&sonarDev);
    
    
    // Start the robot task loop running in a new background thread. The 'true' argument means if it loses
    // connection the task loop stops and the thread exits.
     robot.runAsync(true);
    
    // Connect to the laser(s) if lasers were configured in this robot's parameter
    // file or on the command line, and run laser processing thread if applicable
    // for that laser class.  For the purposes of this demo, add all
    // possible lasers to ArRobot's list rather than just the ones that were
    // connected by this call so when you enter laser mode, you
    // can then interactively choose which laser to use from that list of all
    // lasers mentioned in robot parameters and on command line. Normally,
    // only connected lasers are put in ArRobot's list.
     if (!laserConnector.connectLasers(
           false,  // continue after connection failures
           false,  // add only connected lasers to ArRobot
           true    // add all lasers to ArRobot
     ))
     {
        printf("Warning: Could not connect to laser(s). Set LaserAutoConnect to false in this robot's individual parameter file to disable laser connection.\n");
     }
    
    /* not needed, robot connector will do it by default
     if (!sonarConnector.connectSonars(
           false,  // continue after connection failures
           false,  // add only connected lasers to ArRobot
           true    // add all lasers to ArRobot
     ))
     {
       printf("Could not connect to sonars... exiting\n");
       Aria::exit(2);
     }
    */
    
    // Create and connect to the compass if the robot has one.
     ArTCM2 *compass = compassConnector.create(&robot);
     if(compass && !compass->blockingConnect()) {
       compass = NULL;
     }
     
    // Sleep for 1 second so some messages from the initial responses
    // from robots and cameras and such can catch up
    ArUtil::sleep(1000);

    // Set forward velocity to 50 mm/s
    robot.lock();
    robot.enableMotors();
    robot.setVel(1);
    robot.unlock();
    ArUtil::sleep(1000);
    robot.setVel(0);
    

    //set default font
    currentFont = 2;

    //default rotate flag
    rotate_flag = 1;

    // Init state
    ZeroMemory( g_Controllers, sizeof( CONTROLLER_STATE ) * MAX_CONTROLLERS );

    // Enter the message loop
    bool bGotMsg;
    MSG msg;
    msg.message = WM_NULL;

    while( WM_QUIT != msg.message )
    {
        // Use PeekMessage() so we can use idle time to render the scene and call pEngine->DoWork()
        bGotMsg = ( PeekMessage( &msg, nullptr, 0U, 0U, PM_REMOVE ) != 0 );

        if( bGotMsg )
        {
            // Translate and dispatch the message
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
        else
        {
            UpdateControllerState();
            RenderFrame();
        }
    }

    //clean up ARIA
    Aria::exit(0);

    // Clean up 
    UnregisterClass( L"XInputSample", nullptr );

    CoUninitialize();

    return 0;
}


//-----------------------------------------------------------------------------
HRESULT UpdateControllerState()
{
    DWORD dwResult;
    for( DWORD i = 0; i < MAX_CONTROLLERS; i++ )
    {
        // Simply get the state of the controller from XInput.
        dwResult = XInputGetState( i, &g_Controllers[i].state );

        if( dwResult == ERROR_SUCCESS )
            g_Controllers[i].bConnected = true;
        else
            g_Controllers[i].bConnected = false;
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
void RenderFrame()
{
    bool bRepaint = false;

    WCHAR sz[4][1024];
    WORD wButtons;
    for( DWORD i = 0; i < MAX_CONTROLLERS; i++ )
    {
        if( g_Controllers[i].bConnected )
        {
            wButtons = g_Controllers[i].state.Gamepad.wButtons;

            if( g_bDeadZoneOn )
            {
                // Zero value if thumbsticks are within the dead zone 
                if( ( g_Controllers[i].state.Gamepad.sThumbLX < INPUT_DEADZONE &&
                      g_Controllers[i].state.Gamepad.sThumbLX > -INPUT_DEADZONE ) &&
                    ( g_Controllers[i].state.Gamepad.sThumbLY < INPUT_DEADZONE &&
                      g_Controllers[i].state.Gamepad.sThumbLY > -INPUT_DEADZONE ) )
                {
                    g_Controllers[i].state.Gamepad.sThumbLX = 0;
                    g_Controllers[i].state.Gamepad.sThumbLY = 0;
                }

                if( ( g_Controllers[i].state.Gamepad.sThumbRX < INPUT_DEADZONE &&
                      g_Controllers[i].state.Gamepad.sThumbRX > -INPUT_DEADZONE ) &&
                    ( g_Controllers[i].state.Gamepad.sThumbRY < INPUT_DEADZONE &&
                      g_Controllers[i].state.Gamepad.sThumbRY > -INPUT_DEADZONE ) )
                {
                    g_Controllers[i].state.Gamepad.sThumbRX = 0;
                    g_Controllers[i].state.Gamepad.sThumbRY = 0;
                }
            }

            swprintf_s( sz[i], 1024,
                              L"Controller %u: Connected\n"
                              L"  Pressed Buttons: %s%s%s%s%s%s%s%s%s%s%s%s%s%s\n"
                              L"  Left Trigger: %u\n"
                              L"  Right Trigger: %u\n"
                              L"  Left Thumbstick: %d/%d\n"
                              L"  Right Thumbstick: %d/%d", i,
                              ( wButtons & XINPUT_GAMEPAD_DPAD_UP ) ? L"DPAD_UP " : L"",
                              ( wButtons & XINPUT_GAMEPAD_DPAD_DOWN ) ? L"DPAD_DOWN " : L"",
                              ( wButtons & XINPUT_GAMEPAD_DPAD_LEFT ) ? L"DPAD_LEFT " : L"",
                              ( wButtons & XINPUT_GAMEPAD_DPAD_RIGHT ) ? L"DPAD_RIGHT " : L"",
                              ( wButtons & XINPUT_GAMEPAD_START ) ? L"START " : L"",
                              ( wButtons & XINPUT_GAMEPAD_BACK ) ? L"BACK " : L"",
                              ( wButtons & XINPUT_GAMEPAD_LEFT_THUMB ) ? L"LEFT_THUMB " : L"",
                              ( wButtons & XINPUT_GAMEPAD_RIGHT_THUMB ) ? L"RIGHT_THUMB " : L"",
                              ( wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER ) ? L"LEFT_SHOULDER " : L"",
                              ( wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER ) ? L"RIGHT_SHOULDER " : L"",
                              ( wButtons & XINPUT_GAMEPAD_A ) ? L"A " : L"",
                              ( wButtons & XINPUT_GAMEPAD_B ) ? L"B " : L"",
                              ( wButtons & XINPUT_GAMEPAD_X ) ? L"X " : L"",
                              ( wButtons & XINPUT_GAMEPAD_Y ) ? L"Y " : L"",
                              g_Controllers[i].state.Gamepad.bLeftTrigger,
                              g_Controllers[i].state.Gamepad.bRightTrigger,
                              g_Controllers[i].state.Gamepad.sThumbLX,
                              g_Controllers[i].state.Gamepad.sThumbLY,
                              g_Controllers[i].state.Gamepad.sThumbRX,
                              g_Controllers[i].state.Gamepad.sThumbRY );
        }
        else
        {
            swprintf_s( sz[i], 1024, L"Controller %u: Not connected", i );
        }

        //start鍵當作reset鍵
        if (wButtons & XINPUT_GAMEPAD_START) {
            robot.lock();
            robot.setVel(0);
            robot.setRotVel(0);
            robot.unlock();
        }

        //change the ArRobot object's rotating velocity
        if (g_Controllers[i].state.Gamepad.bLeftTrigger) {
            robot.lock();
            robot.setRotVel(550);
            robot.unlock();
            ArUtil::sleep(18);
            robot.lock();
            robot.setRotVel(0);
            robot.unlock();
        }
        else if (g_Controllers[i].state.Gamepad.bRightTrigger) {
            robot.lock();
            robot.setRotVel(-550);
            robot.unlock();
            ArUtil::sleep(18);
            robot.lock();
            robot.setRotVel(0);
            robot.unlock();
        }

        //change the ArRobot object's velocity by 方向鍵
        if (wButtons & XINPUT_GAMEPAD_DPAD_UP) {
            robot.lock();
            robot.setVel(250);
            robot.unlock();
            ArUtil::sleep(12);
            robot.setVel(0);
        }
        else if (wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
            robot.lock();
            robot.setVel(-250);
            robot.unlock();
            ArUtil::sleep(12);
            robot.setVel(0);
        }
        else if (wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
            robot.lock();
            robot.setRotVel(210);
            robot.unlock();
            ArUtil::sleep(12);
            robot.setRotVel(0);
        }
        else if (wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
            robot.lock();
            robot.setRotVel(-210);
            robot.unlock();
            ArUtil::sleep(12);
            robot.setRotVel(0);
        }

        //change the ArRobot object's velocity by 左搖桿
        double LX = double(g_Controllers[i].state.Gamepad.sThumbLX);
        double LY = double(g_Controllers[i].state.Gamepad.sThumbLY);
        double cosine = LX / (sqrt(pow(LX, 2) + pow(LY, 2)));
        if (LY > 0 && LX > 0) {
            //第一象限
            if (cosine > CONS2) {
                //0~30度
                robot.lock();
                robot.setVel2(250, 30);
                robot.unlock();
                ArUtil::sleep(ROTATE_DELAY);
                robot.lock();
                robot.setVel2(0, 0);
                robot.unlock();
                //robot.stop();
            }
            else if (cosine < CONS1){
                //60~90度
                // robot.lock();
                // robot.setVel2(0, 0);
                // robot.unlock();
                robot.lock();
                //robot.setVel2(0, 0);
                robot.setVel(250);
                //robot.move(100);
                robot.unlock();
                ArUtil::sleep(30);
                robot.lock();
                robot.setVel(0);
                robot.setVel2(0, 0);
                robot.unlock();
            }
            else {
                //30~60度
                robot.lock();
                robot.setVel2(230, 100);
                robot.unlock();
                ArUtil::sleep(ROTATE_DELAY);
                robot.lock();
                robot.setVel2(0, 0);
                robot.unlock();
                //robot.stop();
            }
        }
        else if (LY > 0 && LX < 0) {
            //第二象限
            if (cosine < -CONS2) {
                //150~180度
                robot.lock();
                robot.setVel2(30, 250);
                robot.unlock();
                ArUtil::sleep(ROTATE_DELAY);
                robot.lock();
                robot.setVel2(0, 0);
                robot.unlock();
                //robot.stop();
            }
            else if (cosine > -CONS1){
                //90~120度
                // robot.lock();
                // robot.setVel2(0, 0);
                // robot.unlock();
                robot.lock();
                //robot.setVel2(0, 0);
                robot.setVel(250);
                //robot.move(100);
                robot.unlock();
                ArUtil::sleep(30);
                robot.lock();
                robot.setVel(0);
                robot.setVel2(0, 0);
                robot.unlock();
            }
            else {
                //120~150度
                robot.lock();
                robot.setVel2(100, 230);
                robot.unlock();
                ArUtil::sleep(ROTATE_DELAY);
                robot.lock();
                robot.setVel2(0, 0);
                robot.unlock();
                //robot.stop();
            }
        }
        if (LY < 0 && LX < 0) {
            //第三象限
            if (cosine < -CONS2) {
                //180~210度
                robot.lock();
                robot.setVel2(-30, -250);
                robot.unlock();
                ArUtil::sleep(ROTATE_DELAY);
                robot.lock();
                robot.setVel2(0, 0);
                robot.unlock();
                //robot.stop();
            }
            else if (cosine > -CONS1){
                //240~270度
                // robot.lock();
                // robot.setVel2(0, 0);
                // robot.unlock();
                robot.lock();
                //robot.setVel2(0, 0);
                robot.setVel(-250);
                //robot.move(100);
                robot.unlock();
                ArUtil::sleep(30);
                robot.lock();
                robot.setVel(0);
                robot.setVel2(0, 0);
                robot.unlock();
            }
            else {
                //210~240度
                robot.lock();
                robot.setVel2(-100, -230);
                robot.unlock();
                ArUtil::sleep(ROTATE_DELAY);
                robot.lock();
                robot.setVel2(0, 0);
                robot.unlock();
                //robot.stop();
            }
        }
        else if (LY < 0 && LX > 0) {
            //第四象限
            if (cosine > CONS2) {
                //330~360度
                robot.lock();
                robot.setVel2(-250, -30);
                robot.unlock();
                ArUtil::sleep(ROTATE_DELAY);
                robot.lock();
                robot.setVel2(0, 0);
                robot.unlock();
                //robot.stop();
            }
            else if (cosine < CONS1){
                //270~300度
                // robot.lock();
                // robot.setVel2(0, 0);
                // robot.unlock();
                robot.lock();
                //robot.setVel2(0, 0);
                robot.setVel(-250);
                //robot.move(100);
                robot.unlock();
                ArUtil::sleep(30);
                robot.lock();
                robot.setVel(0);
                robot.setVel2(0, 0);
                robot.unlock();
            }
            else {
                //300~330度
                robot.lock();
                robot.setVel2(-230, -100);
                robot.unlock();
                ArUtil::sleep(ROTATE_DELAY);
                robot.lock();
                robot.setVel2(0, 0);
                robot.unlock();
                //robot.stop();
            }
        }
        //robot.stop();

        //change the ArRobot object's velocity by 右搖桿
        if (rotate_flag) {
            LX = double(g_Controllers[i].state.Gamepad.sThumbRX);
            LY = double(g_Controllers[i].state.Gamepad.sThumbRY);
            cosine = LX / (sqrt(pow(LX, 2) + pow(LY, 2)));
            if (LY > 0 && LX > 0) {
                //第一象限
                if (cosine > CONS2) {
                    //0~30度
                    robot.lock();
                    robot.setDeltaHeading(-90);
                    robot.unlock();
                }
                else if (cosine < CONS1){
                    //60~90度
                    robot.lock();
                    robot.setDeltaHeading(0);
                    robot.unlock();
                }
                else {
                    //30~60度
                    robot.lock();
                    robot.setDeltaHeading(-45);
                    robot.unlock();
                }
            }
            else if (LY > 0 && LX < 0) {
                //第二象限
                if (cosine < -CONS2) {
                    //150~180度
                    robot.lock();
                    robot.setDeltaHeading(90);
                    robot.unlock();
                }
                else if (cosine > -CONS1){
                    //90~120度
                    robot.lock();
                    robot.setDeltaHeading(0);
                    robot.unlock();
                }
                else {
                    //120~150度
                    robot.lock();
                    robot.setDeltaHeading(45);
                    robot.unlock();
                }
            }
            if (LY < 0 && LX < 0) {
                //第三象限
                if (cosine < -CONS2) {
                    //180~210度
                    robot.lock();
                    robot.setDeltaHeading(90);
                    robot.unlock();
                }
                else if (cosine > -CONS1){
                    //240~270度
                    robot.lock();
                    robot.setDeltaHeading(180);
                    robot.unlock();
                }
                else {
                    //210~240度
                    robot.lock();
                    robot.setDeltaHeading(135);
                    robot.unlock();
                }
            }
            else if (LY < 0 && LX > 0) {
                //第四象限
                if (cosine > CONS2) {
                    //330~360度
                    robot.lock();
                    robot.setDeltaHeading(-90);
                    robot.unlock();
                }
                else if (cosine < CONS1){
                    //270~300度
                    robot.lock();
                    robot.setDeltaHeading(-180);
                    robot.unlock();
                }
                else {
                    //300~330度
                    robot.lock();
                    robot.setDeltaHeading(-135);
                    robot.unlock();
                }
            }
        }

        if( wcscmp( sz[i], g_szMessage[i] ) != 0 )
        {
            wcscpy_s( g_szMessage[i], 1024, sz[i] );
            bRepaint = true;
        }
    }

    if( bRepaint )
    {
        // Repaint the window if needed 
        InvalidateRect( g_hWnd, nullptr, TRUE );
        UpdateWindow( g_hWnd );
    }

    // This sample doesn't use Direct3D.  Instead, it just yields CPU time to other 
    // apps but this is not typically done when rendering
    Sleep( 10 );
}


//-----------------------------------------------------------------------------
// Window message handler
//-----------------------------------------------------------------------------
LRESULT WINAPI MsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    static HWND buttonA, buttonB, buttonC;
    HFONT font;

    switch( msg )
    {
        case WM_ACTIVATEAPP:
        {
#if (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/) || defined(USE_DIRECTX_SDK)

            //
            // XInputEnable is implemented by XInput 1.3 and 1.4, but not 9.1.0
            //

            if( wParam == TRUE )
            {
                // App is now active, so re-enable XInput
                XInputEnable( TRUE );
            }
            else
            {
                // App is now inactive, so disable XInput to prevent
                // user input from effecting application and to 
                // disable rumble. 
                XInputEnable( FALSE );
            }

#endif
            break;
        }

        case WM_KEYDOWN:
        {
            if( wParam == 'D' ) g_bDeadZoneOn = !g_bDeadZoneOn;
            break;
        }

        case WM_CREATE:
        {
            HFONT font;
            //create three buttons
            buttonA = CreateWindow(L"Button" , L"Deadzone On" , WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,  
                535, 10, 160, 50, hWnd, (HMENU)BUTTON_ONE, hg_app, NULL);  
            buttonB = CreateWindow(L"Button" , L"Easy Rotate On" , WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,  
                535, 70, 160, 50, hWnd, (HMENU)BUTTON_TWO, hg_app, NULL);
            buttonC = CreateWindow(WC_COMBOBOX, TEXT("Fonts"), CBS_SIMPLE | CBS_DROPDOWN | CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE,
                535, 130, 135, 200, hWnd, NULL, hg_app, NULL);

            // load the combobox with item list.  
            // Send a CB_ADDSTRING message to load each item
            TCHAR Fonts[7][16] =  
            {
                TEXT("Arial"), TEXT("Times New Roman"), TEXT("Monaco"), TEXT("Impact"), TEXT("Helvetica"), TEXT("Georgia"), TEXT("Gotham")
            };

            TCHAR A[16]; 

            memset(&A, 0, sizeof(A));       
            for (int k = 0; k <= 6; k++)
            {
                wcscpy_s(A, sizeof(A) / sizeof(TCHAR), (TCHAR*)Fonts[k]);

                // Add string to combobox.
                SendMessage(buttonC, (UINT)CB_ADDSTRING, (WPARAM)2, (LPARAM)A); 
            }
  
            // Send the CB_SETCURSEL message to display an initial item 
            // in the selection field  
            SendMessage(buttonC, CB_SETCURSEL, (WPARAM)3, (LPARAM)0);
        }

        case WM_COMMAND:
        {  
            if (HIWORD(wParam) == CBN_SELCHANGE)
            // If the user makes a selection from the list:
            // Send CB_GETCURSEL message to get the index of the selected list item.
            // Send CB_GETLBTEXT message to get the item.
            // Display the item in a messagebox.
            { 
                int ItemIndex = SendMessage((HWND)lParam, (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
                TCHAR  ListItem[256];
                (TCHAR) SendMessage((HWND)lParam, (UINT)CB_GETLBTEXT, (WPARAM)ItemIndex, (LPARAM)ListItem);
                if (ListItem[0] == 'A')
                {
                    //MessageBox(hWnd, (LPCWSTR)ListItem, TEXT("Item Selected"), MB_OK);
                    currentFont = 1;
                }
                else if (ListItem[0] == 'T')
                {
                    //MessageBox(hWnd, (LPCWSTR)ListItem, TEXT("Item Selected"), MB_OK);
                    currentFont = 2;
                }
                else if (ListItem[0] == 'M')
                {
                    currentFont = 3;
                }
                else if (ListItem[0] == 'I')
                {
                    currentFont = 4;
                }
                else if (ListItem[0] == 'H')
                {
                    //MessageBox(hWnd, (LPCWSTR)ListItem, TEXT("Item Selected"), MB_OK);
                    currentFont = 5;
                }
                else if (ListItem[0] == 'G')
                {
                    currentFont = 6;
                }
                else if (ListItem[0] == 'P')
                {
                    currentFont = 7;
                }
                //MessageBox(hWnd, (LPCWSTR)ListItem, TEXT("Item Selected"), MB_OK);                        
            }

            switch (LOWORD(wParam))  
            {  
                case BUTTON_ONE:
                    //MessageBox(hwnd, L"您點擊了第一個按鈕。", L"提示", MB_OK | MB_ICONINFORMATION);  
                    if (button_flag[0] == 0) {
                        SendMessage((HWND)lParam, WM_SETTEXT, (WPARAM)NULL, (LPARAM)L"Deadzone Off");
                        button_flag[0] = 1;
                        g_bDeadZoneOn = FALSE;
                    }
				    else if (button_flag[0] == 1) {
                        SendMessage((HWND)lParam, WM_SETTEXT, (WPARAM)NULL, (LPARAM)L"Deadzone On");
                        button_flag[0] = 0;
                        g_bDeadZoneOn = TRUE;
                    }
                    break;

                case BUTTON_TWO:
                    //MessageBox(hwnd, L"您點擊了第二個按鈕。", L"提示", MB_OK | MB_ICONINFORMATION);  
                    if (button_flag[1] == 0) {
                        SendMessage((HWND)lParam, WM_SETTEXT, (WPARAM)NULL, (LPARAM)L"Easy Rotate Off");
                        button_flag[1] = 1;
                        rotate_flag = 0;
                    }
                    else if (button_flag[1] == 1) {
                        SendMessage((HWND)lParam, WM_SETTEXT, (WPARAM)NULL, (LPARAM)L"Easy Rotate On");
                        button_flag[1] = 0;
                        rotate_flag = 1;
                    }  
                    break;

			    case BUTTON_THREE:
                    //MessageBox(hwnd, L"您點擊了第三個按鈕。", L"提示", MB_OK | MB_ICONINFORMATION);  
                    if (button_flag[2] == 0) {
                        SendMessage((HWND)lParam, WM_SETTEXT, (WPARAM)NULL, (LPARAM)L"Third button clicked");
                        button_flag[2] = 1;
                    }
                    else if (button_flag[2] == 1) {
                        SendMessage((HWND)lParam, WM_SETTEXT, (WPARAM)NULL, (LPARAM)L"Unclicked third");
                        button_flag[2] = 0;
                    }  
                    break;

                default:
                    break;
            }  
        }

        case WM_PAINT:
        {
            // Paint some simple explanation text
            PAINTSTRUCT ps;
            HDC hDC = BeginPaint( hWnd, &ps );
            SetBkColor( hDC, backgroundColor );
            SetTextColor( hDC, textColor );
            RECT rect;
            GetClientRect( hWnd, &rect );

            TCHAR FontDictionary[7][16] =  
            {
                TEXT("Arial"), TEXT("Times New Roman"), TEXT("Monaco"), TEXT("Impact"), TEXT("Helvetica"), TEXT("Georgia"), TEXT("Gotham")
            };
 
            font = CreateFont(0, 0, 0, 0,
                            FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
                            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, FontDictionary[currentFont-1]);

            SelectObject(hDC, font);

            rect.top = 15;
            rect.left = 20;
            DrawText( hDC,
                      L"You can connect upto 4 controllers.\nIf you turn on 'Easy Rotate'\nLeft trigger will represent rotating counter-clockwise\nRight trigger will represent rotating clockwise\nPress 'D' to toggle dead zone clamping.", -1, &rect, 0 );

            for( DWORD i = 0; i < MAX_CONTROLLERS; i++ )
            {
                rect.top = i * 120 + 120;
                rect.left = 20;
                DrawText( hDC, g_szMessage[i], -1, &rect, 0 );
            }

            EndPaint( hWnd, &ps );
            return 0;
        }

        case WM_DESTROY:
        {
            PostQuitMessage( 0 );
            break;
        }
    }

    return DefWindowProc( hWnd, msg, wParam, lParam );
}
