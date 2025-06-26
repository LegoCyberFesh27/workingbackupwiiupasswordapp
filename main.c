#include <wut.h>
#include <coreinit/screen.h>
#include <coreinit/thread.h>
#include <coreinit/memory.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/launch.h>
#include <coreinit/systeminfo.h>
#include <vpad/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_INPUT 12
#define TIMEOUT_SECONDS 30

// Password sequence: Right, Down, Up, Up, Left, Down, Down, Up, Down
int password[] = {
    VPAD_BUTTON_RIGHT, VPAD_BUTTON_DOWN, VPAD_BUTTON_UP, VPAD_BUTTON_UP, 
    VPAD_BUTTON_LEFT, VPAD_BUTTON_DOWN, VPAD_BUTTON_DOWN, VPAD_BUTTON_UP, VPAD_BUTTON_DOWN
};
int password_len = 9;
int input_sequence[MAX_INPUT];
int input_index = 0;
int timeout_counter = 0;
bool screen_initialized = false;

bool validate_password() {
    if (input_index != password_len) return false;
    for (int i = 0; i < password_len; i++) {
        if (input_sequence[i] != password[i]) return false;
    }
    return true;
}

void reset_input() {
    input_index = 0;
    memset(input_sequence, 0, sizeof(input_sequence));
}

void cleanup_and_exit() {
    if (screen_initialized) {
        // Clear screens before exit
        OSScreenClearBufferEx(SCREEN_TV, 0x000000);
        OSScreenClearBufferEx(SCREEN_DRC, 0x000000);
        OSScreenFlipBuffersEx(SCREEN_TV);
        OSScreenFlipBuffersEx(SCREEN_DRC);
        
        // Clean shutdown
        VPADShutdown();
        OSScreenShutdown();
    }
}

int main(int argc, char **argv) {
    // Initialize screen subsystem
    OSScreenInit();
    size_t tvBufferSize = OSScreenGetBufferSizeEx(SCREEN_TV);
    size_t drcBufferSize = OSScreenGetBufferSizeEx(SCREEN_DRC);
    
    void *tvBuffer = MEMAllocFromDefaultHeapEx(tvBufferSize, 0x100);
    void *drcBuffer = MEMAllocFromDefaultHeapEx(drcBufferSize, 0x100);
    
    if (!tvBuffer || !drcBuffer) {
        // Critical error - cannot allocate screen buffers
        if (tvBuffer) MEMFreeToDefaultHeap(tvBuffer);
        if (drcBuffer) MEMFreeToDefaultHeap(drcBuffer);
        return -1;
    }
    
    OSScreenSetBufferEx(SCREEN_TV, tvBuffer);
    OSScreenSetBufferEx(SCREEN_DRC, drcBuffer);
    OSScreenEnableEx(SCREEN_TV, 1);
    OSScreenEnableEx(SCREEN_DRC, 1);
    screen_initialized = true;
    
    // Initialize input subsystem
    VPADInit();
    
    VPADStatus status;
    VPADReadError error;
    
    // Main password entry loop
    while (true) {
        // Clear both screens
        OSScreenClearBufferEx(SCREEN_TV, 0x000000);
        OSScreenClearBufferEx(SCREEN_DRC, 0x000000);
        
        // Display title
        OSScreenPutFontEx(SCREEN_TV, 0, 0, "DEVICE LOCKED - Enter Password");
        OSScreenPutFontEx(SCREEN_DRC, 0, 0, "DEVICE LOCKED - Enter Password");
        
        // Show password progress
        char progress[64];
        sprintf(progress, "Sequence: ");
        for (int i = 0; i < password_len; i++) {
            if (i < input_index) {
                strcat(progress, "*");
            } else {
                strcat(progress, "_");
            }
            if (i < password_len - 1) strcat(progress, " ");
        }
        OSScreenPutFontEx(SCREEN_TV, 0, 2, progress);
        OSScreenPutFontEx(SCREEN_DRC, 0, 2, progress);
        
        // Display instructions
        OSScreenPutFontEx(SCREEN_TV, 0, 4, "D-Pad: Enter sequence");
        OSScreenPutFontEx(SCREEN_DRC, 0, 4, "D-Pad: Enter sequence");
        OSScreenPutFontEx(SCREEN_TV, 0, 5, "+: Submit   HOME: Reset");
        OSScreenPutFontEx(SCREEN_DRC, 0, 5, "+: Submit   HOME: Reset");
        
        // Show timeout warning
        int remaining_seconds = TIMEOUT_SECONDS - (timeout_counter / 20); // 20 = ~1 second at 50ms intervals
        if (remaining_seconds > 0) {
            char timeout_msg[64];
            sprintf(timeout_msg, "Auto-continue in %d seconds", remaining_seconds);
            OSScreenPutFontEx(SCREEN_TV, 0, 7, timeout_msg);
            OSScreenPutFontEx(SCREEN_DRC, 0, 7, timeout_msg);
        }
        
        // Flip screen buffers to display content
        OSScreenFlipBuffersEx(SCREEN_TV);
        OSScreenFlipBuffersEx(SCREEN_DRC);
        
        // Read controller input
        VPADRead(VPAD_CHAN_0, &status, 1, &error);
        
        if (error == VPAD_READ_SUCCESS) {
            uint32_t pressed = status.trigger;
            
            // Reset timeout on any input
            if (pressed != 0) {
                timeout_counter = 0;
            }
            
            // Handle d-pad input for password entry
            if (input_index < password_len) {
                if (pressed & VPAD_BUTTON_UP) {
                    input_sequence[input_index++] = VPAD_BUTTON_UP;
                }
                else if (pressed & VPAD_BUTTON_DOWN) {
                    input_sequence[input_index++] = VPAD_BUTTON_DOWN;
                }
                else if (pressed & VPAD_BUTTON_LEFT) {
                    input_sequence[input_index++] = VPAD_BUTTON_LEFT;
                }
                else if (pressed & VPAD_BUTTON_RIGHT) {
                    input_sequence[input_index++] = VPAD_BUTTON_RIGHT;
                }
            }
            
            // Handle submission
            if (pressed & VPAD_BUTTON_PLUS) {
                if (validate_password()) {
                    // Password correct - show success and exit
                    OSScreenClearBufferEx(SCREEN_TV, 0x00FF00);
                    OSScreenClearBufferEx(SCREEN_DRC, 0x00FF00);
                    OSScreenPutFontEx(SCREEN_TV, 0, 8, "PASSWORD ACCEPTED");
                    OSScreenPutFontEx(SCREEN_DRC, 0, 8, "PASSWORD ACCEPTED");
                    OSScreenPutFontEx(SCREEN_TV, 0, 10, "Continuing boot...");
                    OSScreenPutFontEx(SCREEN_DRC, 0, 10, "Continuing boot...");
                    OSScreenFlipBuffersEx(SCREEN_TV);
                    OSScreenFlipBuffersEx(SCREEN_DRC);
                    OSSleepTicks(OSMillisecondsToTicks(2000));
                    
                    // Clean up and exit successfully
                    cleanup_and_exit();
                    MEMFreeToDefaultHeap(tvBuffer);
                    MEMFreeToDefaultHeap(drcBuffer);
                    return 0;
                } else {
                    // Password incorrect - show error
                    OSScreenClearBufferEx(SCREEN_TV, 0xFF0000);
                    OSScreenClearBufferEx(SCREEN_DRC, 0xFF0000);
                    OSScreenPutFontEx(SCREEN_TV, 0, 8, "INCORRECT PASSWORD");
                    OSScreenPutFontEx(SCREEN_DRC, 0, 8, "INCORRECT PASSWORD");
                    OSScreenPutFontEx(SCREEN_TV, 0, 10, "Try again...");
                    OSScreenPutFontEx(SCREEN_DRC, 0, 10, "Try again...");
                    OSScreenFlipBuffersEx(SCREEN_TV);
                    OSScreenFlipBuffersEx(SCREEN_DRC);
                    OSSleepTicks(OSMillisecondsToTicks(2000));
                    
                    // Reset input and continue
                    reset_input();
                }
            }
            
            // Handle reset
            if (pressed & VPAD_BUTTON_HOME) {
                reset_input();
            }
        }
        
        // Increment timeout counter
        timeout_counter++;
        
        // Check for timeout
        if (timeout_counter >= (TIMEOUT_SECONDS * 20)) { // 20 = ~1 second at 50ms intervals
            // Timeout reached - show message and continue
            OSScreenClearBufferEx(SCREEN_TV, 0xFFFF00);
            OSScreenClearBufferEx(SCREEN_DRC, 0xFFFF00);
            OSScreenPutFontEx(SCREEN_TV, 0, 8, "SECURITY TIMEOUT");
            OSScreenPutFontEx(SCREEN_DRC, 0, 8, "SECURITY TIMEOUT");
            OSScreenPutFontEx(SCREEN_TV, 0, 10, "Continuing boot anyway...");
            OSScreenPutFontEx(SCREEN_DRC, 0, 10, "Continuing boot anyway...");
            OSScreenFlipBuffersEx(SCREEN_TV);
            OSScreenFlipBuffersEx(SCREEN_DRC);
            OSSleepTicks(OSMillisecondsToTicks(2000));
            
            // Clean up and exit
            cleanup_and_exit();
            MEMFreeToDefaultHeap(tvBuffer);
            MEMFreeToDefaultHeap(drcBuffer);
            return 0;
        }
        
        // Small delay to prevent excessive CPU usage
        OSSleepTicks(OSMillisecondsToTicks(50));
    }
    
    // Should never reach here, but clean up just in case
    cleanup_and_exit();
    MEMFreeToDefaultHeap(tvBuffer);
    MEMFreeToDefaultHeap(drcBuffer);
    return 0;
}