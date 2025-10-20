/*******************************************************************************
  MPLAB Harmony Application Source File

  Company:
    Microchip Technology Inc.

  File Name:
    app.c

  Summary:
    This file contains the source code for the MPLAB Harmony application.

  Description:
    This file contains the source code for the MPLAB Harmony application.  It
    implements the logic of the application's state machine and it may call
    API routines of other MPLAB Harmony modules in the system, such as drivers,
    system services, and middleware.  However, it does not call any of the
    system interfaces (such as the "Initialize" and "Tasks" functions) of any of
    the modules in the system or make any assumptions about when those functions
    are called.  That is the responsibility of the configuration-specific system
    files.
 *******************************************************************************/

//#include "peripheral/systick/plib_systick.h"


#include "peripheral/port/plib_port.h"


// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include "app.h"
#include "gfx/legato/widget/image/legato_widget_image.h"

#define SERVER_PORT 9760
#define CLIENT_15_PORT 9765 // 192.168.100.15 클라이언트의 수신 포트
#define CLIENT_12_PORT 58954 // 192.168.100.12 클라이언트의 수신 포트
// #define CLIENT_PORT 9761 // 클라이언트 소켓을 위한 포트 (서버와 달라야 함)
// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************
/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    This structure should be initialized by the APP_Initialize function.

    Application strings and buffers are be defined outside this structure.
*/

APP_DATA appData;
uint32_t gSelectedImg ;
SYS_INP_InputListener gAppInputListener;

// 목적지 IP 주소를 저장하기 위한 변수
// static UDP_SOCKET clientSocket = INVALID_SOCKET;
static IPV4_ADDR clientIPv4;

// 192.168.100.15 클라이언트로 상태를 보낼지 여부를 결정하는 플래그
static bool sendStateToClient15 = false;

// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Functions
// *****************************************************************************
// *****************************************************************************

/* TODO:  Add any necessary callback functions.
*/

// *****************************************************************************
// *****************************************************************************
// Section: Application Local Functions
// *****************************************************************************
// *****************************************************************************

typedef enum
{
    LCD_WAIT_TOUCH,
    LCD_WAIT_CONNECTION

} LCD_STATES;

LCD_STATES lcd_state = LCD_WAIT_TOUCH;
bool sendData = false;
char server_state[40];
/* TODO:  Add any necessary local functions.
*/
void selectImage(uint32_t gS, const char* source);

void touchDownHandler(const SYS_INP_TouchStateEvent* const evt)
{
    

    // 192.168.100.12 클라이언트에게 상태를 전송합니다.
    char lcdMessageBuffer[32];
    if (gSelectedImg)
    {
        strcpy(lcdMessageBuffer, "opened");
    }
    else
    {
        strcpy(lcdMessageBuffer, "closed");
    }
    
    if(appData.socket != INVALID_SOCKET)
    {
        TCPIP_Helper_StringToIPAddress("192.168.100.12", &clientIPv4);
        TCPIP_UDP_DestinationPortSet(appData.socket, CLIENT_12_PORT);
        TCPIP_UDP_DestinationIPAddressSet(appData.socket, IP_ADDRESS_TYPE_IPV4, (IP_MULTI_ADDRESS*)&clientIPv4);
        TCPIP_UDP_ArrayPut(appData.socket, (uint8_t*)lcdMessageBuffer, strlen(lcdMessageBuffer));
        TCPIP_UDP_Flush(appData.socket);
    }

    // 이미지 업데이트 및 상태 메시지 전송을 selectImage 함수로 일원화합니다.
    selectImage(!gSelectedImg, "_from display");

    // 타이머를 시작합니다.
    RTC_Timer32Start();
}


void timeout_handler( RTC_TIMER32_INT_MASK intCause, uintptr_t context ) // called every 1ms
{
    RTC_Timer32Stop();
            
        char systick_buffer[255];
        strcpy(systick_buffer, "led off");
        if(appData.socket != INVALID_SOCKET)
        {
            TCPIP_Helper_StringToIPAddress("192.168.100.12", &clientIPv4);
            TCPIP_UDP_DestinationPortSet(appData.socket, CLIENT_12_PORT);
            // 기존 서버 소켓의 목적지 IP를 동적으로 변경
            TCPIP_UDP_DestinationIPAddressSet(appData.socket, IP_ADDRESS_TYPE_IPV4, (IP_MULTI_ADDRESS*)&clientIPv4);
            TCPIP_UDP_ArrayPut(appData.socket, (uint8_t*)systick_buffer, strlen(systick_buffer));
            TCPIP_UDP_Flush(appData.socket);
        }
        
        SYS_CONSOLE_MESSAGE("led off \r\n");
}

void selectImage(uint32_t gS, const char* source)
{
    // 이미지 상태가 변경되지 않았으면 아무 작업도 하지 않고 반환합니다.
    // if (gSelectedImg == gS)
    // {
    //     // 상태가 동일하므로 메시지를 보낼 필요가 없습니다.
    //     return;
    // }

    // 이미지 상태를 gS 값으로 설정합니다.
    gSelectedImg = gS;

    if ( gS ) {
        Screen0_ImageWidget_0->fn->setImage(Screen0_ImageWidget_0, (leImage*)&imgDoorOpen);
        sprintf(server_state, "server state_open%s", source);
    }
    else {
        Screen0_ImageWidget_0->fn->setImage(Screen0_ImageWidget_0, (leImage*)&imgDoorClose);
        sprintf(server_state, "server state_close%s", source);
    }
    Screen0_ImageWidget_0->fn->update(Screen0_ImageWidget_0, 0) ;

    // 192.168.100.15 클라이언트에게 서버 상태를 알려야 함을 표시합니다.
    sendStateToClient15 = true;
}

// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void APP_Initialize ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Initialize ( void )
{
    /* Place the App state machine in its initial state. */
    appData.state = APP_TCPIP_WAIT_INIT;


    RTC_Timer32CallbackRegister(&timeout_handler, 0);
    RTC_Timer32InterruptEnable(RTC_TIMER32_INT_MASK_CMP0);
//    SYSTICK_TimerStart();

    /* TODO: Initialize your application's state machine and other
     * parameters.
     */
}


/******************************************************************************
  Function:
    void APP_Tasks ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Tasks ( void )
{
    SYS_STATUS          tcpipStat;
    const char          *netName, *netBiosName;
    static IPV4_ADDR    dwLastIP[2] = { {-1}, {-1} };
    IPV4_ADDR           ipAddr;
    int                 i, nNets;
    TCPIP_NET_HANDLE    netH;
    
    /* Check the application's current state. */

    // 192.168.100.15 클라이언트로 상태 메시지를 보냅니다.
    if (sendStateToClient15 && appData.socket != INVALID_SOCKET && TCPIP_UDP_PutIsReady(appData.socket) >= strlen(server_state))
    {
        TCPIP_Helper_StringToIPAddress("192.168.100.15", &clientIPv4);
        // 기존 서버 소켓의 목적지 IP를 동적으로 변경
        TCPIP_UDP_DestinationPortSet(appData.socket, CLIENT_15_PORT);
        TCPIP_UDP_DestinationIPAddressSet(appData.socket, IP_ADDRESS_TYPE_IPV4, (IP_MULTI_ADDRESS*)&clientIPv4);
        TCPIP_UDP_ArrayPut(appData.socket, (uint8_t*)server_state, strlen(server_state));
        TCPIP_UDP_Flush(appData.socket);

        // 메시지를 보낸 후 플래그를 리셋합니다.
        sendStateToClient15 = false;
    }
    
    switch ( appData.state )
    {
        /* Application's initial state. */
        case APP_TCPIP_WAIT_INIT:
        {
//            bool appInitialized = true;

            for( uint32_t aTemp = 0 ; aTemp < 256 ; aTemp ++ ) {
                if (SYS_INP_RemoveListener( aTemp ) == -1 ) break ;
            }
            
            gAppInputListener.handleTouchDown = &touchDownHandler;
    
            SYS_INP_AddListener(&gAppInputListener);
        
//            if (appInitialized)
//            {
//
//                appData.state = APP_STATE_SERVICE_TASKS;
//            }
            
            tcpipStat = TCPIP_STACK_Status(sysObj.tcpip);
            if(tcpipStat < 0)
            {   // some error occurred
                SYS_CONSOLE_MESSAGE(" APP: TCP/IP stack initialization failed!\r\n");
                appData.state = APP_TCPIP_ERROR;
            }
            else if(tcpipStat == SYS_STATUS_READY)
            {
                // now that the stack is ready we can check the 
                // available interfaces 
                nNets = TCPIP_STACK_NumberOfNetworksGet();
                for(i = 0; i < nNets; i++)
                {

                    netH = TCPIP_STACK_IndexToNet(i);
                    netName = TCPIP_STACK_NetNameGet(netH);
                    netBiosName = TCPIP_STACK_NetBIOSName(netH);

#if defined(TCPIP_STACK_USE_NBNS)
                    SYS_CONSOLE_PRINT("    Interface %s on host %s - NBNS enabled\r\n", netName, netBiosName);
#else
                    SYS_CONSOLE_PRINT("    Interface %s on host %s - NBNS disabled\r\n", netName, netBiosName);
#endif  // defined(TCPIP_STACK_USE_NBNS)
                    (void)netName;          // avoid compiler warning 
                    (void)netBiosName;      // if SYS_CONSOLE_PRINT is null macro

                }
                appData.state = APP_TCPIP_WAIT_FOR_IP;
            }
            
            break;
        }

        case APP_TCPIP_WAIT_FOR_IP:

            // if the IP address of an interface has changed
            // display the new value on the system console
            nNets = TCPIP_STACK_NumberOfNetworksGet();

            for (i = 0; i < nNets; i++)
            {
                netH = TCPIP_STACK_IndexToNet(i);
                
				if(!TCPIP_STACK_NetIsReady(netH))
				{
					return; // interface not ready yet!
				}
							
				ipAddr.Val = TCPIP_STACK_NetAddress(netH);
                if(dwLastIP[i].Val != ipAddr.Val)
                {
                    dwLastIP[i].Val = ipAddr.Val;

                    SYS_CONSOLE_MESSAGE(TCPIP_STACK_NetNameGet(netH));
                    SYS_CONSOLE_MESSAGE(" IP Address: ");
                    SYS_CONSOLE_PRINT("%d.%d.%d.%d \r\n", ipAddr.v[0], ipAddr.v[1], ipAddr.v[2], ipAddr.v[3]);                                     
                }
            }
            // all interfaces ready. Could start transactions!!!
            
            appData.state = APP_TCPIP_OPENING_SERVER;  
            break;
			
        case APP_TCPIP_OPENING_SERVER:
        {
            SYS_CONSOLE_PRINT("Waiting for Client Connection on port: %d\r\n", SERVER_PORT);
            appData.socket = TCPIP_UDP_ServerOpen(IP_ADDRESS_TYPE_IPV4, SERVER_PORT, 0);
            if (appData.socket == INVALID_SOCKET)
            {
                SYS_CONSOLE_MESSAGE("Couldn't open server socket\r\n");
                break;
            }
            appData.state = APP_TCPIP_WAIT_FOR_CONNECTION;
        }
        break;

        case APP_TCPIP_WAIT_FOR_CONNECTION:
        {
            if (!TCPIP_UDP_IsConnected(appData.socket))
            {
                return;
            }
            else
            {
                // We got a connection
                appData.state = APP_TCPIP_SERVING_CONNECTION;
                SYS_CONSOLE_MESSAGE("Received a connection\r\n");
//                GPIO_PB03_Set();
            }
        }
        break;

        case APP_TCPIP_SERVING_CONNECTION:
        {
            if (!TCPIP_UDP_IsConnected(appData.socket))
            {
                appData.state = APP_TCPIP_WAIT_FOR_CONNECTION;
                SYS_CONSOLE_MESSAGE("Connection was closed\r\n");
                break;
            }
            
            int16_t wMaxGet, wMaxPut, wCurrentChunk;
            uint16_t w, w2;
            uint8_t AppBuffer[32 + 1];
            memset(AppBuffer, 0, sizeof(AppBuffer));
            // Figure out how many bytes have been received and how many we can transmit.
            wMaxGet = TCPIP_UDP_GetIsReady(appData.socket);	// Get UDP RX FIFO byte count
            wMaxPut = TCPIP_UDP_PutIsReady(appData.socket);

            // 응답을 보내기 전에, 메시지를 보낸 클라이언트의 IP와 포트로 목적지를 다시 설정합니다.
            UDP_SOCKET_INFO sktInfo;
            TCPIP_UDP_SocketInfoGet(appData.socket, &sktInfo);
            
                       
            //SYS_CONSOLE_PRINT("\t%d bytes are available.\r\n", wMaxGet);
            if (wMaxGet == 0)
            {
                break;
            }

            if (wMaxPut < wMaxGet)
            {
                wMaxGet = wMaxPut;
            }
            
            // SYS_CONSOLE_PRINT("RX Buffer has %d bytes in it\n", wMaxGet);

            // Process all bytes that we can
            // This is implemented as a loop, processing up to sizeof(AppBuffer) bytes at a time.
            // This limits memory usage while maximizing performance.  Single byte Gets and Puts are a lot slower than multibyte GetArrays and PutArrays.
            wCurrentChunk = sizeof(AppBuffer) - 1;
            for(w = 0; w < wMaxGet; w += sizeof(AppBuffer) - 1)
            {
                // Make sure the last chunk, which will likely be smaller than sizeof(AppBuffer), is treated correctly.
                if(w + sizeof(AppBuffer) - 1 > wMaxGet)
                    wCurrentChunk = wMaxGet - w;

                // Transfer the data out of the TCP RX FIFO and into our local processing buffer.
                int rxed = TCPIP_UDP_ArrayGet(appData.socket, AppBuffer, sizeof(AppBuffer) - 1);

                SYS_CONSOLE_PRINT("\tReceived a message of '%s' and length %d\r\n", AppBuffer, rxed);

                const char* source_suffix = "";
                char ip_buffer[20];
                TCPIP_Helper_IPAddressToString(&sktInfo.remoteIPaddress.v4Add, ip_buffer, sizeof(ip_buffer));
                if (strcmp(ip_buffer, "192.168.100.12") == 0)
                {
                    source_suffix = "_from key";
                }
                else if (strcmp(ip_buffer, "192.168.100.15") == 0)
                {
                    source_suffix = "_from app";
                }
                else
                { 
                    source_suffix = "";
                }

                if (strcmp((char *)AppBuffer, "button111 pressed_from Client") == 0)
                {
                    
                    SYS_CONSOLE_PRINT("\t\tbutton1 pressed!!!\r\n");
                    selectImage(1, source_suffix); // Open
                }
                else
                {
                    selectImage(0, source_suffix); // Close
                }
            

                // Perform the "ToUpper" operation on each data byte
                for(w2 = 0; w2 < wCurrentChunk; w2++)
                {
                    i = AppBuffer[w2];
                    if(i == '\x1b')   // escape
                    {
                        SYS_CONSOLE_MESSAGE("Connection was closed\r\n");
                    }
                }
                AppBuffer[w2] = 0;
                SYS_CONSOLE_PRINT("\tSending a message: '%s'\r\n", AppBuffer);


                // TCPIP_UDP_DestinationIPAddressSet(appData.socket, sktInfo.addressType, (IP_MULTI_ADDRESS*)&sktInfo.remoteIPaddress);
                // TCPIP_UDP_DestinationPortSet(appData.socket, sktInfo.remotePort);
                TCPIP_UDP_ArrayPut(appData.socket, AppBuffer, wCurrentChunk);

                
                // send message from server - without new connection
                char APP_Message_Buffer[255];
                strcpy(APP_Message_Buffer, "\r\n message from server");

                // 여기에서도 동일하게 목적지를 설정해 주어야 합니다.
                // TCPIP_UDP_DestinationIPAddressSet(appData.socket, sktInfo.addressType, (IP_MULTI_ADDRESS*)&sktInfo.remoteIPaddress);
                // TCPIP_UDP_DestinationPortSet(appData.socket, sktInfo.remotePort);
                TCPIP_UDP_ArrayPut(appData.socket, (uint8_t*)APP_Message_Buffer, strlen(APP_Message_Buffer));
                
                appData.state = APP_TCPIP_WAIT_FOR_CONNECTION;
            }
            TCPIP_UDP_Flush(appData.socket);
//            TCPIP_UDP_Discard(appData.socket);
        }
        break;

        default:
        {
            break;
        }
    }
}