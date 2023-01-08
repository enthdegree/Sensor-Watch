/*
 * MIT License
 *
 * Copyright (c) 2023 Christian Chapman
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "morsecalc_face.h"
#include "watch.h"
#include "watch_utility.h"
#include "watch_private_display.h"

// Display float on screen
void morsecalc_print_float(double d) { 
    // Special cases 
    if(d == 0) {
        watch_display_string("     0", 4); 
        return;
    }
    else if(isnan(d)) {
        watch_display_string("   nan", 4);
        return;
    }
    else if(d == (1.0)/(0.0)) {
        watch_display_string("   inf", 4);
        return;
    }
    else if(d == (-1.0)/(0.0)) {
        watch_display_character('-', 1);
        watch_display_string("   inf", 4);
        return;
    }

    // Record number properties
    // Sign
    int is_negative = d<0;
    if(is_negative) d = -d; 

    // Order of magnitude
    int om = (int) floor(log(d)/log(10));
    int om_is_negative = (om<0);

    // Get the first 4 significant figures
    int digits;
    digits = round(d*pow(10.0, 3-om));
    if(digits>9999) {
        digits = 1000;
        om++;
    }

    // Print signs
    if(is_negative) watch_display_character('-', 1);
    else watch_display_character(' ', 1); 
    if(om_is_negative < 0) watch_display_character('-', 2); 
    else watch_display_character(' ', 2); 

    // Print first 4 significant figures
    watch_display_character('0'+(digits/1000)%10, 4);
    watch_display_character('0'+(digits/100 )%10, 5);
    watch_display_character('0'+(digits/10  )%10, 6);
    watch_display_character('0'+(digits/1   )%10, 7);

    // Prinat exponent
    if(om_is_negative) om = -om; // Make exponent positive for display
    if(om<=99) {
        watch_display_character('0'+(om/10  )%10, 8);
        watch_display_character('0'+(om/1   )%10, 9);
    } else { // Over/underflow
        if(om_is_negative) watch_display_string("    uf", 4);
        else watch_display_string("    of", 4);
        if(om<9999) { // Use main display to show order of magnitude
            // (Should always succeed; max double is <2e308)
            watch_display_character('0'+(om/1000)%10, 4);
            watch_display_character('0'+(om/100 )%10, 5);
            watch_display_character('0'+(om/10  )%10, 6);
            watch_display_character('0'+(om/1   )%10, 7);
        }
    }
    return;
}

// Print current input token
void morsecalc_print_token(morsecalc_state_t *mcs) {
    watch_display_string("          ", 0); // Clear display

    // Print morse code buffer
    char c = mc_dec(mcs->mc->b); // Decode the morse code buffer's current contents
    if('\0' == c) c = ' '; // Needed for watch_display_character
    watch_display_character(c, 0); // Display current morse code char in mode position
    watch_display_character('0'+(mcs->mc->bidx), 3); // Display buffer position in top right

    // Print last 6 chars of current input line 
    uint8_t nlen = strlen(mcs->token); // number of characters in token
    uint8_t nprint = min(nlen,6); // number of characters to print
    watch_display_string(mcs->token+nlen-nprint, 10-nprint); // print right-aligned
    return;
}

// Clear token buffer
void morsecalc_reset_token(morsecalc_state_t *mcs) { 
    memset(mcs->token, '\0', MORSECALC_TOKEN_LEN*sizeof(mcs->token[0]));
    mcs->idxt = 0;
    return;
}

// Print stack contents. 
void morsecalc_print_stack(morsecalc_state_t * mcs) {
    watch_display_string("          ", 0); // Clear display

    // If the morse code buffer has a numeral in it, print that stack item 
    // Otherwise print top of stack
    char c = mc_dec(mcs->mc->b); 
    uint8_t idx = 0;
    if(c >= '0' && c <= '9') idx = c - '0';

    if(idx >= mcs->cs->s) watch_display_string(" empty", 4); // Stack empty
    else morsecalc_print_float(mcs->cs->stack[mcs->cs->s-1-idx]); // Print stack item

    watch_display_character('0'+idx, 0); // Print which stack item this is top center
    watch_display_character('0'+(mcs->cs->s), 3); // Print the # of stack items top right 
    return;
}

// Write something into the morse code buffer.
// Input: c = dot (0), dash (1), or 'complete' ('x')
void morsecalc_input(morsecalc_state_t * mcs, char c) {
    int status = 0;
    if( c != 'x' ) { // Dot or dash received
        mc_input(mcs->mc, c);
        morsecalc_print_token(mcs);
    } 
    else { // Morse code character finished
        char dec = mc_dec(mcs->mc->b); 
        mc_reset(mcs->mc);
        switch(dec) {
            case '\0': // Invalid character, do nothing
                morsecalc_print_token(mcs);
                break;
                
            case ' ': // Submit token to calculator
                status = calc_input(mcs->cs, mcs->token);
                morsecalc_reset_token(mcs); 
                morsecalc_print_stack(mcs);   
                break;
                
            case '(': // -.--. Erase previous character in token
                if(mcs->idxt>0) {
                    mcs->idxt--;
                    mcs->token[mcs->idxt] = '\0';
                }
                morsecalc_print_token(mcs);
                break;
                
            case 'S': // -.-.- Erase entire token without submitting
                morsecalc_reset_token(mcs); 
                morsecalc_print_stack(mcs);   
                break;
                
            default: // Add character to token
                if(mcs->idxt < MORSECALC_TOKEN_LEN-1) {
                    mcs->token[mcs->idxt] = dec;
                    mcs->idxt++; 
                    morsecalc_print_token(mcs);
                }
                else  watch_display_string("  full", 4); 
                break;
        }
    }
    
    // Print errors if there are any
    switch(status) {
        case  0: break; // Success
        case -1: watch_display_string("cmderr", 4); break; // Unrecognized command
        case -2: watch_display_string("stkerr", 4); break; // Bad stack size
        default: watch_display_string("   err", 4); break; // Other error 
    }
    
    return;    
}

void morsecalc_face_setup(movement_settings_t *settings, uint8_t watch_face_index, void ** context_ptr) {
    (void) settings;
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(morsecalc_state_t)); 
        morsecalc_state_t *mcs = (morsecalc_state_t *)*context_ptr;
        morsecalc_reset_token(mcs); 
        
        mcs->cs = (calc_state_t *) malloc(sizeof(calc_state_t));
        calc_init(mcs->cs); 
        
        mcs->mc = (mc_state_t *) malloc(sizeof(mc_state_t));
        mc_reset(mcs->mc);
    }
    return;
}

void morsecalc_face_activate(movement_settings_t *settings, void *context) {
    (void) settings;
    morsecalc_state_t *mcs = (morsecalc_state_t *) context;
    mc_reset(mcs->mc);
    morsecalc_print_stack(mcs);
    return;
}

bool morsecalc_face_loop(movement_event_t event, movement_settings_t *settings, void *context) {
    (void) settings;
    morsecalc_state_t *mcs = (morsecalc_state_t *) context;
    switch(event.event_type) {
    // quit
    case EVENT_TIMEOUT:
        movement_move_to_next_face();
        break;
    case EVENT_MODE_LONG_PRESS:
        movement_move_to_next_face();
        break;

    case EVENT_LIGHT_LONG_PRESS:
        morsecalc_print_stack(mcs);
        break;
    case EVENT_ALARM_LONG_PRESS:
        morsecalc_print_stack(mcs);
        break;
    
    // input
    case EVENT_ALARM_BUTTON_UP:
    // dot
        morsecalc_input(mcs, '.');    
        break;
    case EVENT_LIGHT_BUTTON_UP:
    // dash
        morsecalc_input(mcs, '-'); 
        break;
    case EVENT_MODE_BUTTON_UP:
    // submit character
        morsecalc_input(mcs, 'x'); 
        break;
    }
    
    return true;
}

void morsecalc_face_resign(movement_settings_t *settings, void *context) {
    (void) settings;
    (void) context;
    return;
}

