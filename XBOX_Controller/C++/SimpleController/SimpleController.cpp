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
#include <commdlg.h>
#include <basetsd.h>
#include <objbase.h>

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

    //set default font
    currentFont = 2;

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
    for( DWORD i = 0; i < MAX_CONTROLLERS; i++ )
    {
        if( g_Controllers[i].bConnected )
        {
            WORD wButtons = g_Controllers[i].state.Gamepad.wButtons;

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
            buttonB = CreateWindow(L"Button" , L"Easy Rotate Off" , WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,  
                535, 70, 160, 50, hWnd, (HMENU)BUTTON_TWO, hg_app, NULL);
            buttonC = CreateWindow(WC_COMBOBOX, TEXT("Fonts"), CBS_SIMPLE | CBS_DROPDOWN | CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE,
                535, 130, 135, 200, hWnd, NULL, hg_app, NULL);

            // load the combobox with item list.  
            // Send a CB_ADDSTRING message to load each item
            TCHAR Fonts[7][16] =  
            {
                TEXT("Arial"), TEXT("Times New Roman"), TEXT("Monaco"), TEXT("Impact"), TEXT("Helvetica"), TEXT("Georgia"), TEXT("Palatino")
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
                        SendMessage((HWND)lParam, WM_SETTEXT, (WPARAM)NULL, (LPARAM)L"Easy Rotate On");
                        button_flag[1] = 1;
                    }
                    else if (button_flag[1] == 1) {
                        SendMessage((HWND)lParam, WM_SETTEXT, (WPARAM)NULL, (LPARAM)L"Easy Rotate Off");
                        button_flag[1] = 0;
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
                TEXT("Arial"), TEXT("Times New Roman"), TEXT("Monaco"), TEXT("Impact"), TEXT("Helvetica"), TEXT("Georgia"), TEXT("Palatino")
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
