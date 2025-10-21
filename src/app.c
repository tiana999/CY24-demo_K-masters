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

// --- Constants for Network Configuration ---
#define SERVER_PORT 9760
#define CLIENT_15_PORT 9765 // 192.168.100.15 클라이언트의 수신 포트
#define CLIENT_12_PORT 58954 // 192.168.100.12 클라이언트의 수신 포트

#define CLIENT_12_IP "192.168.100.12"
#define CLIENT_15_IP "192.168.100.15"

// --- Constants for Messages ---
static const char* CMD_FOR_12_GREEN_LED_ON = "opened";   // Command for 192.168.100.12 to turn Green LED ON
static const char* CMD_FOR_12_RED_LED_ON = "closed";  // Command for 192.168.100.12 to turn Red LED ON
static const char* CMD_FOR_12_ALL_LEDS_OFF = "led off"; // Command for 192.168.100.12 to turn all LEDs OFF
static const char* MSG_SERVER_STATE_OPEN_FORMAT = "server state_open%s";
static const char* MSG_SERVER_STATE_CLOSE_FORMAT = "server state_close%s";
static const char* MSG_FROM_CLIENT_BUTTON_PRESS = "button111 pressed_from Client";

// --- Buffer Sizes ---
#define MAX_MESSAGE_BUFFER_SIZE 40
#define MAX_IP_STRING_SIZE 20

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
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
bool isDoorOpen = false; // Replaced gSelectedImg for clarity
SYS_INP_InputListener gAppInputListener;

// --- Private state variables ---
static bool sendStateToClient15 = false; // Flag to send status to client 15
static char serverStateMessage[MAX_MESSAGE_BUFFER_SIZE];

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

/* TODO:  Add any necessary local functions.
*/

/**
 * @brief Updates the door image on the UI based on the door's state.
 * @param open True if the door is open, false otherwise.
 */
static void UpdateDoorImage(bool open)
{
    leImage* image = open ? (leImage*)&imgDoorOpen : (leImage*)&imgDoorClose;
    Screen0_ImageWidget_0->fn->setImage(Screen0_ImageWidget_0, image);
    Screen0_ImageWidget_0->fn->update(Screen0_ImageWidget_0, 0);
}

void updateDoorState(bool open, const char* source);

static bool SendUdpMessage(const char* ipAddress, UDP_PORT port, const uint8_t* message, size_t length)
{
    if (appData.socket == INVALID_SOCKET || TCPIP_UDP_PutIsReady(appData.socket) < length)
    {
        return false;
    }

    IPV4_ADDR destIP;
    TCPIP_Helper_StringToIPAddress(ipAddress, &destIP);

    TCPIP_UDP_DestinationIPAddressSet(appData.socket, IP_ADDRESS_TYPE_IPV4, (IP_MULTI_ADDRESS*)&destIP);
    TCPIP_UDP_DestinationPortSet(appData.socket, port);
    
    TCPIP_UDP_ArrayPut(appData.socket, message, length);
    TCPIP_UDP_Flush(appData.socket);
    
    return true;
}

void touchDownHandler(const SYS_INP_TouchStateEvent* const evt)
{
    // 192.168.100.12 클라이언트에게 상태를 전송합니다.
    const char* ledCommand;
    if (!isDoorOpen) // 문이 닫혀있다가 터치로 열릴 것이므로 LED ON 명령 전송
    {
        ledCommand = CMD_FOR_12_GREEN_LED_ON;
    }
    else
    {
        ledCommand = CMD_FOR_12_RED_LED_ON;
    }
    SendUdpMessage(CLIENT_12_IP, CLIENT_12_PORT, (const uint8_t*)ledCommand, strlen(ledCommand));

    // 이미지 업데이트 및 상태 메시지 전송을 selectImage 함수로 일원화합니다.
    updateDoorState(!isDoorOpen, "_from display");

    // 타이머를 시작합니다.
    RTC_Timer32Start();
}


void timeout_handler( RTC_TIMER32_INT_MASK intCause, uintptr_t context )
{
    RTC_Timer32Stop();
            
    if (SendUdpMessage(CLIENT_12_IP, CLIENT_12_PORT, (const uint8_t*)CMD_FOR_12_ALL_LEDS_OFF, strlen(CMD_FOR_12_ALL_LEDS_OFF)))
    {
        SYS_CONSOLE_MESSAGE("led off \r\n");
    }
}

void updateDoorState(bool open, const char* source)
{
    // 이미지 상태가 변경되지 않았으면 아무 작업도 하지 않고 반환합니다.
    // if (isDoorOpen == open)
    // {
    //     return;
    // }

    isDoorOpen = open;

    UpdateDoorImage(isDoorOpen);

    if (isDoorOpen) {
        sprintf(serverStateMessage, MSG_SERVER_STATE_OPEN_FORMAT, source);
    } else {
        sprintf(serverStateMessage, MSG_SERVER_STATE_CLOSE_FORMAT, source);
    }

    // 192.168.100.15 클라이언트에게 서버 상태를 알려야 함을 표시합니다.
    sendStateToClient15 = true;
}

static const char* GetSourceSuffixFromIP(const char* ipAddress)
{
    if (strcmp(ipAddress, CLIENT_12_IP) == 0)
    {
        return "_from key";
    }
    else if (strcmp(ipAddress, CLIENT_15_IP) == 0)
    {
        return "_from app";
    }
    return ""; // Default: no suffix
}

static void ProcessUdpMessage(const uint8_t* buffer, const UDP_SOCKET_INFO* sktInfo)
{
    char ipBuffer[MAX_IP_STRING_SIZE];
    TCPIP_Helper_IPAddressToString(&sktInfo->remoteIPaddress.v4Add, ipBuffer, sizeof(ipBuffer));
    const char* sourceSuffix = GetSourceSuffixFromIP(ipBuffer);

    if (strcmp((char *)buffer, MSG_FROM_CLIENT_BUTTON_PRESS) == 0)
    {
        SYS_CONSOLE_PRINT("\t\tbutton1 pressed!!!\r\n");
        updateDoorState(true, sourceSuffix); // Open
    }
    else
    {
        updateDoorState(false, sourceSuffix); // Close
    }
}

static void SendUdpResponse(const uint8_t* buffer, int length)
{
    const char* serverMessage = "\r\n message from server";
    size_t serverMessageLen = strlen(serverMessage);
    size_t totalLength = length + serverMessageLen;

    // Check if there is enough space in the TX buffer before putting data.
    if (TCPIP_UDP_PutIsReady(appData.socket) < totalLength)
    {
        SYS_CONSOLE_MESSAGE("\tUDP TX buffer is not ready for the response.\r\n");
        return;
    }

    SYS_CONSOLE_PRINT("\tSending a response: '%.*s' and server message\r\n", length, buffer);

    TCPIP_UDP_ArrayPut(appData.socket, buffer, length); // Echo back
    TCPIP_UDP_ArrayPut(appData.socket, (const uint8_t*)serverMessage, serverMessageLen); // Additional server message

    // Send all queued data for this response
    TCPIP_UDP_Flush(appData.socket);
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
    if (sendStateToClient15)
    {
        if (SendUdpMessage(CLIENT_15_IP, CLIENT_15_PORT, (const uint8_t*)serverStateMessage, strlen(serverStateMessage)))
        {
            // 메시지를 보낸 후 플래그를 리셋합니다.
            sendStateToClient15 = false;
        }
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
            
            int16_t wMaxGet;
            uint8_t AppBuffer[MAX_MESSAGE_BUFFER_SIZE];

            // Process all datagrams in the RX queue
            if((wMaxGet = TCPIP_UDP_GetIsReady(appData.socket)) > 0)
            {
                UDP_SOCKET_INFO sktInfo;
                TCPIP_UDP_SocketInfoGet(appData.socket, &sktInfo);
                memset(AppBuffer, 0, sizeof(AppBuffer));

                // Transfer the data out of the UDP RX FIFO and into our local processing buffer.
                int rxed = TCPIP_UDP_ArrayGet(appData.socket, AppBuffer, sizeof(AppBuffer) - 1);

                if (rxed > 0)
                {
                    SYS_CONSOLE_PRINT("\tReceived a message of '%s' and length %d\r\n", AppBuffer, rxed);
                    ProcessUdpMessage(AppBuffer, &sktInfo);
                    // Send response messages (echo + server message)
                    SendUdpResponse(AppBuffer, rxed);
                }
            }
            appData.state = APP_TCPIP_WAIT_FOR_CONNECTION; // Go back to waiting after processing all packets
        }
        break;

        default:
        {
            break;
        }
    }
}