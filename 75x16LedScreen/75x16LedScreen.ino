/* text file parser ***************************************************
 * 
 * teensy 4.1
 * 
 * */

#define DEBUG true

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))
#define GET_MILLIS millis

#define		FILENAMEFORMAT		"SNG%d.TXT"

#define 	PARSER_TEXT_LEN		255	// MAX size of a file line 
#define 	DISPLAY_TEXT_LENGHT	8	//in letters max
#define		DISPLAY_CHAR_WIDTH	12	//in pixels
#define		SCREEN_REFRESH_RATE 5	// in millis (5ms=200Hz; 10ms => 100Hz; 20ms => 50Hz and so on)

#define 	NEO_LED_PIN 		6
#define		SCREEN_MAX_X		75	//in led nb
#define		SCREEN_MAX_Y		16	//in led nb
#define		MAX_BRIGHTNESS		127	//255 max

#define		BITMAPNAMEFORMAT	"IMG%d/IMG%d.PPM"
#define		ANIMNAMEFORMAT		"ANIM/ANIM%d.PPM"
#define 	PARSER_PPM_LEN		70 // MAX size of a file line

#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>
#include <SD.h>
#include <SPI.h>
#include <MIDI.h>
#include "ParserLib.h"
#include <SerialCommand.h> // parse serial command

#include "some_colors.h"

//instanciate objects *************************************************

// led matrix
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(SCREEN_MAX_X, SCREEN_MAX_Y, NEO_LED_PIN,
  NEO_MATRIX_TOP     + NEO_MATRIX_LEFT +
  NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG,
  NEO_GRB            + NEO_KHZ800);

// canvas (video buffer)
GFXcanvas16 canvas(SCREEN_MAX_X, SCREEN_MAX_Y); // Create canvas object for .PPM
GFXcanvas16 canvasX10(SCREEN_MAX_X, SCREEN_MAX_Y * 10); // Create canvas object for anim

// old way Midi  
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1,  MIDI);

// Serial command
SerialCommand sCmd;

// GLOBAL VARs (are bad)
File Global_SD_file;
File Global_SD_bitmap_file;

char parser_str[PARSER_TEXT_LEN];
Parser parser((byte*)parser_str, PARSER_TEXT_LEN);

char PPMparser_str[PARSER_PPM_LEN];
Parser PPMparser((byte*)PPMparser_str, PARSER_PPM_LEN);

uint8_t		Global_brightness=127;
uint32_t	Global_file_pos=0;
uint32_t	Global_bitmap_file_pos=0;
uint16_t	Global_scroll_speed=10;
uint16_t	Global_data1=0;
uint16_t	Global_data2=0;
uint16_t	Global_data3=0;
boolean		Global_flag_auto_next=0;
uint16_t	Global_flag_auto_next_wait=0;
boolean		Global_flag_loop=true;
boolean		Global_flag_next_text_ready=0;
char		Global_next_text_str[PARSER_TEXT_LEN];
uint16_t	Global_next_text_lenght=0;
int 		Global_scroll_x=0;
uint8_t		Global_scroll_char_start=0;
uint16_t	Global_background_color=0;
uint16_t	Global_text_color=LED_WHITE_HIGH;
uint16_t	Global_strobe_color=LED_WHITE_HIGH;
uint16_t	Global_strobe_speed=120;
uint16_t	Global_fade_speed=60;
uint32_t	Global_TS_beat8_start=0;
uint8_t		Global_SD_bitmap_folderNb=0;
uint8_t		Global_SD_bitmap_fileNb=0;
uint16_t	Global_SD_bitmap_speed=60;
uint16_t	Global_SD_Anim_pos=0;
uint16_t	Global_glitter_amount=10; //in pixels /pass
uint16_t	Global_glitter_speed=50; //in millis() should stay longer than screen refresh
uint16_t	Global_glitter_color=18;  //purple medium
elapsedMillis	Global_elapsed_matrix_refresh=0;
elapsedMillis	Global_elapsed_scroll_time =0;
elapsedMillis	Global_elapsed_glitter=0;
elapsedMillis	Global_flag_auto_next_elapsed=0;


//declare screen macros
void	dummy(); //do Nothing
void	macro_clearAll();
void	parse_display_next_text();
void	macro_background_color_red();
void	macro_background_color_green();
void	macro_background_color_blue();
void	macro_background_color_white();
void	macro_background_color_black();
void	macro_background_color_table();
void	macro_text_color_red();
void	macro_text_color_green();
void	macro_text_color_blue();
void	macro_text_color_white();
void 	macro_text_color_black();
void	macro_text_color_table();
void	macro_strobe_background_color();
void	macro_fadeIN_fadeOUT();
void	macro_fadeIN();
void	macro_fadeOUT();
void	macro_display_SD_PPM();
void	macro_display_SD_mult_PPM();
void	macro_display_SD_anim_PPM();
void 	macro_glitter();

typedef void (*MacroList[])();

MacroList lMacros = 
{
	dummy,
	macro_clearAll,
	parse_display_next_text,
	macro_background_color_red,
	macro_background_color_green,
	macro_background_color_blue,
	macro_background_color_white,
	macro_background_color_black,
	macro_background_color_table,
	macro_text_color_red,
	macro_text_color_green,
	macro_text_color_blue,
	macro_text_color_white,
	macro_text_color_black,
	macro_text_color_table,
	macro_strobe_background_color,
	macro_fadeIN_fadeOUT,
	macro_fadeIN,
	macro_fadeOUT,
	macro_display_SD_PPM,
	macro_display_SD_mult_PPM,
	macro_display_SD_anim_PPM,
	macro_glitter
}; 

//enum macro that needs refresh
enum{
	NOREFRESH,
	STROBE,
	FADE_IN_OUT,
	FADE_IN,
	FADE_OUT,
	PPM,
	PPM_AUTO,
	ANIM,
	GLITTER
};

uint16_t Global_macro_need_refresh=0;

//declare refreshable macros 
void refreshMacroStrobe();
void refreshMacroFadeInOut();
void refreshMacroFadeIn();
void refreshMacroFadeOUT();
void refreshMacroDisplayPPM();
void refreshMacroDisplayMultPPM();
void refreshMacroDisplayAnimPPM();
void refreshMacroGlitter();

MacroList lRefreshMacros =
{
	dummy,
	refreshMacroStrobe,
	refreshMacroFadeInOut,
	refreshMacroFadeIn,
	refreshMacroFadeOUT,
	refreshMacroDisplayPPM,
	refreshMacroDisplayMultPPM,
	refreshMacroDisplayAnimPPM,
	refreshMacroGlitter
};

// declare control change 
void cc_set_brightness(byte value);
void cc_set_text_file(byte value);
void cc_set_background_color(byte value);
void cc_set_text_color(byte value);
void cc_set_text_speed(byte value);
void cc_set_text_loop(byte value);
void cc_set_strobe_color(byte value);
void cc_set_strobe_speed(byte value);
void cc_set_fade_speed(byte value);
void cc_set_bitmap_folder(byte value);
void cc_set_bitmap_file(byte value);
void cc_set_bitmap_speed(byte value);
void cc_set_glitter_amount(byte value);
void cc_set_glitter_speed(byte value);
void cc_set_glitter_color(byte value);

typedef void(*CC_List[])(byte value);
CC_List lControlChange =
{
	dummmy,
	cc_set_brightness,
	cc_set_text_file,
	cc_set_background_color,
	cc_set_text_color,
	cc_set_text_speed,
	cc_set_text_loop,
	cc_set_strobe_color,
	cc_set_strobe_speed,
	cc_set_fade_speed,
	cc_set_bitmap_folder,
	cc_set_bitmap_file,
	cc_set_bitmap_speed,
	cc_set_glitter_amount,
	cc_set_glitter_speed,
	cc_set_glitter_color
};

//readline error list
enum{ 
	errorFileNotOpen,
	errorBufferTooSmall,
	errorSeekError,
	errorEndOfFile,
	errorNoError
};

//pre set colors
uint16_t colors_table[] = 
{
	LED_BLACK, //black
	matrix.Color(255, 0, 0),
	matrix.Color(0, 255, 0),
	matrix.Color(0, 0, 255),
	LED_RED_VERYLOW,
	LED_RED_LOW,
	LED_RED_MEDIUM,
	LED_RED_HIGH,
	LED_GREEN_VERYLOW,
	LED_GREEN_LOW,
	LED_GREEN_MEDIUM,
	LED_GREEN_HIGH,
	LED_BLUE_VERYLOW,
	LED_BLUE_LOW,
	LED_BLUE_MEDIUM,
	LED_BLUE_HIGH,
	LED_ORANGE_VERYLOW,
	LED_ORANGE_LOW,
	LED_ORANGE_MEDIUM,
	LED_ORANGE_HIGH,
	LED_PURPLE_VERYLOW,
	LED_PURPLE_LOW,
	LED_PURPLE_MEDIUM,
	LED_PURPLE_HIGH,
	LED_CYAN_VERYLOW,
	LED_CYAN_LOW,
	LED_CYAN_MEDIUM,
	LED_CYAN_HIGH,
	LED_WHITE_VERYLOW,
	LED_WHITE_LOW,
	LED_WHITE_MEDIUM,
	LED_WHITE_HIGH,
	LED_BLACK
};

// wtf ??
bool get_SD_bitmap_raw(uint8_t foldernumber, uint8_t filenumber, uint8_t x, uint8_t y);


//************** INIT routine called in setup***************************

bool init_SD()
{
		Serial.print("Initializing SD card...");
		if (!SD.begin(BUILTIN_SDCARD)) 
		{
			Serial.println("SD initialization failed!");
			return false;
		}
		Serial.println("SD initialization done.");
		return true;
	}


// initialize MIDI *****************************************************
void initMIDISerial() // old fashion MIDI hardware INPUT
{
  // 
  MIDI.setHandleNoteOn(handleNoteOn);
  MIDI.setHandleNoteOff(handleNoteOff);
  //MIDI.setHandlePitchBend(handlePitchBend);
  MIDI.setHandleControlChange(handleControlChange);
  //MIDI.setHandleSystemReset(handleSystemReset);
  MIDI.begin(MIDI_CHANNEL_OMNI);
}


//useful routine *******************************************************
void	dummy() //do Nothing
{
	;
}

uint16_t beat88( uint16_t beats_per_minute, uint32_t timebase = 0)
{
    return (((GET_MILLIS()) - timebase) * beats_per_minute * 65536) / 60000;
}

// beat8 generates an 8-bit 'sawtooth' wave at a given BPM
uint8_t beat8( uint16_t beats_per_minute, uint32_t timebase = 0)
{
    return (beat88( beats_per_minute, timebase))>> 8;
}

// beat8 generates only one 8-bit 'sawtooth' at a given BPM
uint8_t onebeat8( uint16_t beats_per_minute , uint32_t timebase = 0)
{
	uint32_t onePeriod = 60000 / beats_per_minute; 
	uint32_t t=(GET_MILLIS() - timebase);
	if( t >= onePeriod)return 255;
	return(beat88( beats_per_minute, timebase))>> 8;
}

uint8_t scale8( uint8_t i, uint8_t scale)
{
	return (((uint16_t)i) * (1+(uint16_t)(scale))) >> 8;
}

uint8_t dim8_raw( uint8_t x)
{
    return scale8( x, x);
}

uint8_t brighten8_raw( uint8_t x)
{
    uint8_t ix = 255 - x;
    return 255 - scale8( ix, ix);
}

uint8_t ease8InOutQuad( uint8_t i)
{
    uint8_t j = i;
    if( j & 0x80 ) {
        j = 255 - j;
    }
    uint8_t jj  = scale8(  j, j);
    uint8_t jj2 = jj << 1;
    if( i & 0x80 ) {
        jj2 = 255 - jj2;
    }
    return jj2;
}

uint8_t triwave8(uint8_t in)
{
    if( in & 0x80) {
        in = 255 - in;
    }
    uint8_t out = in << 1;
    return out;
}

uint8_t quadwave8(uint8_t in)
{
    return ease8InOutQuad( triwave8(in));
}

uint8_t fadeToBlackBy( uint8_t val, uint8_t fadeBy )
{
    return (( (uint16_t)dim8_raw( 255-fadeBy ) * (uint16_t)val) >> 8 );
}

uint8_t brightToFullBy( uint8_t val, uint8_t brightBy )
{
	return (( (uint16_t)dim8_raw( brightBy ) * (uint16_t)val) >> 8 );
}


//**********************************************************************

// custom readline from txt file **************************************
int readLine(File &afile, char *buffer, size_t len, uint32_t &pos)
{
  if (!afile)
  {
	  
	  if(DEBUG)Serial.println("readline errorFileNotOpen");
		return errorFileNotOpen;
  }
 
  if (len < 3) 
    {
		if(DEBUG)Serial.println("readline errorBufferTooSmall");
		return errorBufferTooSmall;
	}

  if (!afile.seek(pos))
	{
		if(DEBUG)Serial.println("readline errorSeekError");
		return errorSeekError;
	}

  size_t bytesRead = afile.read(buffer, len);
  if (!bytesRead) {
    buffer[0] = '\0';
    //return 1; // done
    //if(DEBUG)Serial.println("readline errorEndOfFile");
    return errorEndOfFile;
  }
  
  for (size_t i = 0; i < bytesRead && i < len-1; ++i) {
    // Test for '\n' with optional '\r' too
    // if (endOfLineTest(buffer, len, i, '\n', '\r')
	
    if (buffer[i] == '\n' || buffer[i] == '\r') {
      char match = buffer[i];
      char otherNewline = (match == '\n' ? '\r' : '\n'); 
      // end of line, discard any trailing character of the other sort
      // of newline
      buffer[i] = '\0';
      
      if (buffer[i+1] == otherNewline)++i;
      pos += (i + 1); // skip past newline(s)
      //return (i+1 == bytesRead && !file.available());
      return errorNoError;
    }
  }
  if (!afile.available()) {
    // end of file without a newline
    buffer[bytesRead] = '\0';
    // return 1; //done
    //if(DEBUG)Serial.println("readline errorEndOfFile");
    return errorEndOfFile;
  }
  
  buffer[len-1] = '\0'; // terminate the string
  if(DEBUG)Serial.println("readline errorBufferTooSmall");
  return errorBufferTooSmall;
}

//**********************************************************************

//*************parse TEXTE routines ************************************

// prepare file to be parse , return false if no file *************
// called on midi event

bool get_text_file(uint16_t filenumber)
{
	char filename[28];
	Global_file_pos=0;

	//sprintf(filename, "%s", USITT_FILENAME);
	sprintf(filename, FILENAMEFORMAT, filenumber);
	
	if(DEBUG)Serial.print("SD opening: " );
	if(DEBUG)Serial.println(filename);
	
	Global_SD_file=SD.open(filename);
	if(! Global_SD_file) 
	{
		if(DEBUG)Serial.println("SD file not exist");
		return false;
	}
	
	//parse_display_next_text();
	
	return true;
}

// get next line of textfile to be parse; return false if no more line *
bool parse_getNextLine()
{
	char strbuffer[PARSER_TEXT_LEN];
	size_t len = PARSER_TEXT_LEN;
	size_t res = 0;
	if( readLine(Global_SD_file, strbuffer, len, Global_file_pos) >= errorNoError)
	{
		//if(DEBUG)Serial.println(strbuffer);
		res =snprintf(parser_str,PARSER_TEXT_LEN,strbuffer);
		parser.Init(parser_str, res, 8);
		return true;
	}
	//here we probably reach the end of the SD text file
	//should close it
	if(DEBUG)Serial.println("Closing SD text file");
	Global_SD_file.close();
	return false;
}

// parse next section of text , called by midi event *******************
void parse_display_next_text()
{
	bool res=false;
	size_t len=8;
	char cmdstrA[len];
	char cmdstrB[len];
	//Global_scroll_speed=500;
	while( parse_getNextLine() && ! Global_flag_next_text_ready)
	{
		parser.SkipWhile(Parser::IsUSITTSeparator);
		if(parser.IfCurrentIs(Parser::IsNewLine) || parser.IfCurrentIs(Parser::IsComment))continue; //ignore line
		
		snprintf(cmdstrA,len-1, "SPD");
		snprintf(cmdstrB,len-1, "spd");
		if(parser.Compare(cmdstrA, 3) || parser.Compare(cmdstrB, 3))
		{
			parser.SkipWhile(Parser::IsUSITTSeparator);
			if(parser.IfCurrentIs(Parser::IsNewLine))continue;
		
			if(parser.IfCurrentIs(Parser::IsDigit) )
			{
				Global_scroll_speed=parser.Read_Uint16();
				continue; //discard rest of line
			}
		}
		
		snprintf(cmdstrA,len-1, "DATA1");
		snprintf(cmdstrB,len-1, "data1");
		if(parser.Compare(cmdstrA, 5) || parser.Compare(cmdstrB, 5))
		{
			parser.SkipWhile(Parser::IsUSITTSeparator);
			if(parser.IfCurrentIs(Parser::IsNewLine))continue;
		
			if(parser.IfCurrentIs(Parser::IsDigit) )
			{
				Global_data1=parser.Read_Uint16();
				continue; //discard rest of line
			}
		}
		
		snprintf(cmdstrA,len-1, "DATA2");
		snprintf(cmdstrB,len-1, "data2");
		if(parser.Compare(cmdstrA, 5) || parser.Compare(cmdstrB, 5))
		{
			parser.SkipWhile(Parser::IsUSITTSeparator);
			if(parser.IfCurrentIs(Parser::IsNewLine))continue;
		
			if(parser.IfCurrentIs(Parser::IsDigit) )
			{
				Global_data2=parser.Read_Uint16();
				continue; //discard rest of line
			}
		}
		
		snprintf(cmdstrA,len-1, "DATA3");
		snprintf(cmdstrB,len-1, "data3");
		if(parser.Compare(cmdstrA, 5) || parser.Compare(cmdstrB, 5))
		{
			parser.SkipWhile(Parser::IsUSITTSeparator);
			if(parser.IfCurrentIs(Parser::IsNewLine))continue;
		
			if(parser.IfCurrentIs(Parser::IsDigit) )
			{
				Global_data3=parser.Read_Uint16();
				continue; //discard rest of line
			}
		}
		
		snprintf(cmdstrA,len-1, "MACRO");
		snprintf(cmdstrB,len-1, "macro");
		if(parser.Compare(cmdstrA, 5) || parser.Compare(cmdstrB, 5))
		{
			parser.SkipWhile(Parser::IsUSITTSeparator);
			if(parser.IfCurrentIs(Parser::IsNewLine))continue;
		
			if(parser.IfCurrentIs(Parser::IsDigit) )
			{
				run_effect_subroutine(parser.Read_Uint16());
				continue; //discard rest of line
			}
		}
		
		snprintf(cmdstrA,len-1, "AUTO");
		snprintf(cmdstrB,len-1, "auto");
		if( parser.Compare(cmdstrA, 4) || parser.Compare(cmdstrB, 4))
		{

			Global_flag_auto_next=1;
			parser.SkipWhile(Parser::IsUSITTSeparator);
			if(parser.IfCurrentIs(Parser::IsNewLine))continue;
		
			if( parser.IfCurrentIs(Parser::IsDigit) )
			{
				Global_flag_auto_next_wait=parser.Read_Uint16();
				Global_flag_auto_next_elapsed=0;
			}
			continue; //discard rest of line
			
		}
		
		snprintf(cmdstrA,len-1, "MAN");
		snprintf(cmdstrB,len-1, "man");
		if(parser.Compare(cmdstrA, 3) || parser.Compare(cmdstrB, 3))
		{

			Global_flag_auto_next=0;
			continue; //discard rest of line
			
		}
		
		snprintf(cmdstrA,len-1, "LOOP");
		snprintf(cmdstrB,len-1, "loop");
		if(parser.Compare(cmdstrA, 4) || parser.Compare(cmdstrB, 4))
		{

			Global_flag_loop=1;
			continue; //discard rest of line
			
		}
		
		snprintf(cmdstrA,len-1, "ULOOP");
		snprintf(cmdstrB,len-1, "uloop");
		if(parser.Compare(cmdstrA, 5) || parser.Compare(cmdstrB, 5))
		{

			Global_flag_loop=0;
			continue; //discard rest of line
			
		}
		
		snprintf(cmdstrA,len-1, "TEXT");
		snprintf(cmdstrB,len-1, "text");
		if(parser.Compare(cmdstrA, 4) || parser.Compare(cmdstrB, 4))
		{

			parser.SkipWhile(Parser::IsUSITTSeparator);
			if(! parse_Text())continue;
			
			Global_flag_next_text_ready=1;
			Global_data1=0; //reset for next macros
			Global_data2=0; //reset for next macros
			Global_data3=0; //reset for next macros
			break;
		}
		
	}//

}

// called by "parse_display_next_text()" , get main text (string)
bool parse_Text()
{
	parser.SkipWhile(Parser::IsUSITTSeparator);
	if(parser.IfCurrentIs(Parser::IsNewLine))
	{
		Global_next_text_str[0]=0;
		Global_next_text_lenght=0;
		return 1;
	}
	size_t res;

	char* CurrentItemPtr=parser.CurrentItemPointer();
	res=parser.Read_CharArray(Parser::IsNewLine);//, true, [](char* data, size_t length,char* strbuffer) { if(DEBUG)Serial.print("test1 ");if(DEBUG)Serial.print(data);if(DEBUG)Serial.println(length);snprintf(strbuffer,length,"%s",data); });
	
	if(res >= ARRAY_SIZE(Global_next_text_str))return 0;// too long str
	
	Global_next_text_lenght=snprintf(Global_next_text_str,res,CurrentItemPtr);
	
	if(Global_next_text_lenght < 0 && Global_next_text_lenght >= res)return 0; // too long str or error
	return 1;
}

//**********************************************************************


//*************parse IMAGE routines ************************************

// get next bitmap line or return false if no more line ****************
bool parse_getNextBitmapLine()
{
	char strbuffer[PARSER_PPM_LEN];
	size_t len = PARSER_PPM_LEN;
	if( readLine(Global_SD_bitmap_file, strbuffer, len, Global_bitmap_file_pos) >= errorNoError)
	{
		//if(DEBUG)Serial.println(strbuffer);
		snprintf(PPMparser_str,PARSER_PPM_LEN,strbuffer);
		PPMparser.Init(PPMparser_str, PARSER_PPM_LEN, 8);
		return true;
	}
	return false;
}

// display ascii .PPM images *************************************************
bool get_SD_bitmap(uint8_t foldernumber, uint8_t filenumber, uint8_t x = 0, uint8_t y = 0)
{
	char filename[28];
	uint8_t ppmsettings[3];
	uint8_t countsettings=0;
	Global_bitmap_file_pos=0;

	sprintf(filename, BITMAPNAMEFORMAT,foldernumber, filenumber);
	
	if(DEBUG)Serial.print("SD opening: " );
	if(DEBUG)Serial.println(filename);
	
	Global_SD_bitmap_file=SD.open(filename);
	if(! Global_SD_bitmap_file) 
	{
		if(DEBUG)Serial.println("SD file not exist");
		return false;
	}
	
	//empty file
	if( ! parse_getNextBitmapLine())
	{
		if(DEBUG)Serial.println("SD file empty");
		Global_SD_bitmap_file.close();
		return false;
	}
	
	//not a PPM ascii 
	char p3str[4];
	snprintf(p3str,3,"P3");
	if( ! PPMparser.Compare(p3str, 2))
	{
		if(DEBUG)Serial.println("SD file must start by P3 ( .ppm ascii)");
		Global_SD_bitmap_file.close();
		// ok let's try raw ( P6 )
		return (get_SD_bitmap_raw(foldernumber, filenumber, x, y));
	}
	
	// get PPM settings 
	while( parse_getNextBitmapLine() && countsettings < 3)
	{
		if(PPMparser.IfCurrentIs(Parser::IsNewLine) || PPMparser.IfCurrentIs(Parser::IsComment))continue;
		
		if(PPMparser.IfCurrentIs(Parser::IsDigit))
		{
			ppmsettings[countsettings]=PPMparser.Read_Uint8();
			//if(DEBUG)Serial.println(ppmsettings[countsettings]);
			countsettings++;
			PPMparser.SkipWhile(Parser::IsUSITTSeparator);
			if(PPMparser.IfCurrentIs(Parser::IsNewLine))continue;
			if(! PPMparser.IfCurrentIs(Parser::IsDigit))break;
			ppmsettings[countsettings]=PPMparser.Read_Uint8();
			//if(DEBUG)Serial.println(ppmsettings[countsettings]);
			countsettings++;
		}
	}
	
	// bad PPM settings
	if( countsettings != 3)
	{
		if(DEBUG)Serial.println("bitmap bad settings count");
		Global_SD_bitmap_file.close();
		return false;
	}
	
	//get pixels from file
	uint8_t x_cpt=0;
	uint8_t y_cpt=0;
	uint8_t PPMpixel[3];
	countsettings=0;
	canvas.fillScreen(0);
	while( parse_getNextBitmapLine() )
	{
		if(PPMparser.IfCurrentIs(Parser::IsNewLine) || PPMparser.IfCurrentIs(Parser::IsComment))continue;
		if(PPMparser.IfCurrentIs(Parser::IsDigit))
		{
			PPMpixel[countsettings]=PPMparser.Read_Uint8();
			countsettings++;
			if(countsettings == 3)// pixel completed
			{
				countsettings=0;
				//display
				if(( x + x_cpt <= SCREEN_MAX_X ) && ( y + y_cpt <= SCREEN_MAX_Y ))
				{
					canvas.drawPixel(x + x_cpt, y + y_cpt, matrix.Color(PPMpixel[0],PPMpixel[1],PPMpixel[2]));
				}
				
				x_cpt++;
				
				if(x_cpt >= ppmsettings[0])
				{
					x_cpt=0;
					y_cpt++;
					if(y_cpt >= ppmsettings[1])break; // we reach the end of image
				}
			}
		}
	}
	Global_SD_bitmap_file.close();
	return true;
}

// display hexa .PPM images *************************************************
bool get_SD_bitmap_raw(uint8_t foldernumber, uint8_t filenumber, uint8_t x = 0, uint8_t y = 0)
{
	char filename[28];
	long int ppmsettings[3];
	//Global_bitmap_file_pos=0;
	char buffer[16];
	uint8_t i=0;
	char * pEnd;

	//sprintf(filename, "%s", USITT_FILENAME);
	sprintf(filename, BITMAPNAMEFORMAT,foldernumber, filenumber);
	
	if(DEBUG)Serial.print("SD opening: " );
	if(DEBUG)Serial.println(filename);
	
	Global_SD_bitmap_file=SD.open(filename);
	if(! Global_SD_bitmap_file) 
	{
		if(DEBUG)Serial.println("SD file not exist");
		Global_SD_bitmap_file.close();
		return false;
	}

	/*
	if (!Global_SD_bitmap_file.seek(Global_bitmap_file_pos))
	{
		if(DEBUG)Serial.println("readline errorSeekError");
		Global_SD_bitmap_file.close();
		return errorSeekError;
	}
	* */
	
	//P6
	if( Global_SD_bitmap_file.read(buffer,3) != 3 || buffer[0]!=0x50 || buffer[1]!=0x36 || buffer[2]!=0x0A)
	{
		if(DEBUG)Serial.println("SD error: read bad header");
		
		return get_SD_bitmap(foldernumber, filenumber, x, y); //try P3 routine
	}
	
	if(DEBUG)
	{
		Serial.print(buffer[0]);
		Serial.println(buffer[1]);
	}
	
	//#
	if(Global_SD_bitmap_file.read() == 0x23)
	{
		while( Global_SD_bitmap_file.read() != 0x0A)
		{
			; //seeking thru comment
		}
	}
		
	//read width & height
	while((buffer[i]=Global_SD_bitmap_file.read()) != 0x0A)
	{
		i++;
		if(i >= ARRAY_SIZE(buffer))
		{
			if(DEBUG)Serial.println("SD error: bitmap header too long? ");
			Global_SD_bitmap_file.close();
			return false;
		}
	}

	ppmsettings[0]=strtol(buffer,&pEnd,10);
	ppmsettings[1]=strtol(pEnd,&pEnd,10);
	
	//read color space should be 255
	i=0;
	while((buffer[i]=Global_SD_bitmap_file.read()) != 0x0A)
	{
		i++;
		if(i>=ARRAY_SIZE(buffer))
		{
			if(DEBUG)Serial.println("SD error: bitmap header too long? ");
			Global_SD_bitmap_file.close();
			return false;
		}
	}
	
	ppmsettings[2]=(uint16_t)strtol(buffer,&pEnd,10);
	
	if(ppmsettings[2] != 255)
	{
		if(DEBUG)Serial.println("SD error: bitmap header bad color ");
		Global_SD_bitmap_file.close();
		return false;
	}
	//end of header parser
	if(DEBUG)
	{
		Serial.println(ppmsettings[0]);
		Serial.println(ppmsettings[1]);
		Serial.println(ppmsettings[2]);
	}
	
	//go get pixels
	uint8_t x_cpt=0;
	uint8_t y_cpt=0;
	canvas.fillScreen(0);
	while( Global_SD_bitmap_file.read(buffer, size_t(3) ) == 3 )
	{
		if(( x + x_cpt <= SCREEN_MAX_X ) && ( y + y_cpt <= SCREEN_MAX_Y ))
		{
			canvas.drawPixel(x + x_cpt, y + y_cpt, matrix.Color(buffer[0],buffer[1],buffer[2]));
		}
		
		x_cpt++;
		
		if(x_cpt >= ppmsettings[0])
		{
			x_cpt=0;
			y_cpt++;
			if(y_cpt >= ppmsettings[1])break; // we reach the end of image
		}
	}
	if(DEBUG)Serial.println("SD ok: bitmap ready ");
	Global_SD_bitmap_file.close();
	return true;
}

// display anim raw .PPM images *************************************************
bool get_SD_bitmap_anim(uint8_t foldernumber, uint8_t filenumber, uint8_t x = 0, uint8_t y = 0)
{
	char filename[36];
	long int ppmsettings[3];
	//Global_bitmap_file_pos=0;
	char buffer[32];
	uint8_t i=0;
	char * pEnd;

	//sprintf(filename, "%s", USITT_FILENAME);
	//sprintf(filename, ANIMNAMEFORMAT,foldernumber, filenumber);
	sprintf(filename, ANIMNAMEFORMAT, filenumber);
	Global_SD_bitmap_file.close();
	
	if(DEBUG)Serial.print("SD opening: " );
	if(DEBUG)Serial.println(filename);
	
	Global_SD_bitmap_file=SD.open(filename);
	if(! Global_SD_bitmap_file) 
	{
		if(DEBUG)Serial.println("SD file not exist");
		Global_SD_bitmap_file.close();
		return false;
	}
	if(DEBUG)Serial.print("SD opening OK " );

	if (!Global_SD_bitmap_file.seek(0))
	{
		if(DEBUG)Serial.println("readline errorSeekError");
		Global_SD_bitmap_file.close();
		return errorSeekError;
	}
	
	//P6
	if( Global_SD_bitmap_file.read(buffer,3) != 3 || buffer[0]!=0x50 || buffer[1]!=0x36 || buffer[2]!=0x0A)
	{
		if(DEBUG)Serial.println("SD error: read bad header");
		Global_SD_bitmap_file.close();
		return false; //
	}
	
	if(DEBUG)
	{
		Serial.print(buffer[0]);
		Serial.println(buffer[1]);
	}
	
	//#
	if(Global_SD_bitmap_file.read() == 0x23)
	{
		if(DEBUG)Serial.println("SD info:skip comments ");
		while( Global_SD_bitmap_file.read() != 0x0A)
		{
			; //seeking thru comment
		}
	}
		
	//read width & height
	while((buffer[i]=Global_SD_bitmap_file.read()) != 0x0A)
	{
		i++;
		if(i >= ARRAY_SIZE(buffer))
		{
			if(DEBUG)Serial.println("SD error: bitmap header too long? ");
			Global_SD_bitmap_file.close();
			return false;
		}
	}

	ppmsettings[0]=strtol(buffer,&pEnd,10);
	ppmsettings[1]=strtol(pEnd,&pEnd,10);
	
	//read color space should be 255
	i=0;
	while((buffer[i]=Global_SD_bitmap_file.read()) != 0x0A)
	{
		i++;
		if(i>=ARRAY_SIZE(buffer))
		{
			if(DEBUG)Serial.println("SD error: bitmap header too long? ");
			Global_SD_bitmap_file.close();
			return false;
		}
	}
	
	ppmsettings[2]=(uint16_t)strtol(buffer,&pEnd,10);
	
	if(ppmsettings[2] != 255)
	{
		if(DEBUG)Serial.println("SD error: bitmap header bad color ");
		Global_SD_bitmap_file.close();
		return false;
	}
	//end of header parser
	if(DEBUG)
	{
		Serial.println(ppmsettings[0]);
		Serial.println(ppmsettings[1]);
		Serial.println(ppmsettings[2]);
	}
	
	//go get pixels
	uint16_t x_cpt=0;
	uint16_t y_cpt=0;
	canvasX10.fillScreen(0);
	while( Global_SD_bitmap_file.read(buffer, size_t(3) ) == 3 )
	{
		if(( x + x_cpt <= SCREEN_MAX_X ) && ( y + y_cpt <= SCREEN_MAX_Y * 10 ))
		{
			canvasX10.drawPixel(x + x_cpt, y + y_cpt, matrix.Color(buffer[0],buffer[1],buffer[2]));
		}
		
		x_cpt++;
		
		if(x_cpt >= ppmsettings[0])
		{
			x_cpt=0;
			y_cpt++;
			if(y_cpt >= ppmsettings[1])break; // we reach the end of image
		}
	}
	if(DEBUG)Serial.println("SD ok: anim ready ");
	Global_SD_bitmap_file.close();
	return true;
}

//**********************************************************************


//***** Screen effects MACROs ******************************************

//called by "parse_display_next_text()" ,run listed macros subroutine***
void run_effect_subroutine(uint16_t nb)
{
	if(nb < ARRAY_SIZE(lMacros))lMacros[nb]();
}

void macro_clearAll()
{
	Global_macro_need_refresh = NOREFRESH;
	Global_background_color=0;
	Global_text_color=65535;
	Global_next_text_str[0]=0;
	Global_next_text_lenght=1;
	Global_flag_next_text_ready=true;
	matrix.fillScreen(0);
	matrix.setCursor(0, 0);
}

void macro_background_color_red()
{
	Global_background_color=matrix.Color(255, 0, 0);
}

void macro_background_color_green()
{
	Global_background_color=(matrix.Color(0, 255, 0));
}

void macro_background_color_blue()
{
	Global_background_color=(matrix.Color(0, 0, 255));
}

void macro_background_color_white()
{
	Global_background_color=(matrix.Color(255, 255, 255));
}

void macro_background_color_black()
{
	Global_background_color=(matrix.Color(0, 0, 0));
}

void macro_background_color_table()
{
	if( Global_data1 > 0 && Global_data1 < ARRAY_SIZE(colors_table))Global_background_color=colors_table[Global_data1];
}

void macro_text_color_red()
{
	Global_text_color=(matrix.Color(255, 0, 0));
}

void macro_text_color_green()
{
	Global_text_color=(matrix.Color(0, 255, 0));
}

void macro_text_color_blue()
{
	Global_text_color=(matrix.Color(0, 0, 255));
}

void macro_text_color_white()
{
	Global_text_color=(matrix.Color(255, 255, 255));
}

void macro_text_color_table()
{
	if( Global_data1 > 0 && Global_data1 < ARRAY_SIZE(colors_table))Global_text_color=colors_table[Global_data1];
}

void macro_text_color_black()
{
	Global_text_color=(matrix.Color(0, 0, 0));
}

void macro_strobe_background_color() //toggle strobe
{
	//uint8_t speed=240; //in BPM
	
	if(Global_macro_need_refresh == STROBE)
	{
		Global_macro_need_refresh = NOREFRESH; // stop refresh
	}else{
		Global_macro_need_refresh = STROBE;
		if( Global_data1 > 0 && Global_data1 < ARRAY_SIZE(colors_table))Global_strobe_color=colors_table[Global_data1];
		if( Global_data2 > 0)Global_strobe_speed=Global_data2;
	}
}

void macro_fadeIN_fadeOUT() //toggle
{
	if(Global_macro_need_refresh == FADE_IN_OUT)
	{
		Global_macro_need_refresh = NOREFRESH; // stop refresh
	}else{
		Global_macro_need_refresh = FADE_IN_OUT;
		if( Global_data1 > 0)Global_fade_speed=Global_data1;
		Global_TS_beat8_start=GET_MILLIS();
	}
}

void macro_fadeIN() //toggle
{
	if(Global_macro_need_refresh == FADE_IN)
	{
		Global_macro_need_refresh = NOREFRESH; // stop refresh
	}else{
		Global_macro_need_refresh = FADE_IN;
		if( Global_data1 > 0)Global_fade_speed=Global_data1;
		Global_TS_beat8_start=GET_MILLIS();
	}
}

void macro_fadeOUT() //toggle
{
	if(Global_macro_need_refresh == FADE_OUT)
	{
		Global_macro_need_refresh = NOREFRESH; // stop refresh
	}else{
		Global_macro_need_refresh = FADE_OUT;
		if( Global_data1 > 0)Global_fade_speed=Global_data1;
		Global_TS_beat8_start=GET_MILLIS();
	}
}

void macro_display_SD_PPM()
{
	if(Global_macro_need_refresh == PPM)
	{
		Global_macro_need_refresh = NOREFRESH; // stop refresh
	}else{
		if( Global_data1 > 0)Global_SD_bitmap_folderNb=Global_data1;
		if( Global_data2 > 0)Global_SD_bitmap_fileNb=Global_data2;
		
		if(get_SD_bitmap_raw(Global_SD_bitmap_folderNb, Global_SD_bitmap_fileNb, 0, 0))
		{
			Global_macro_need_refresh = PPM;
		}
		
	}
}

void macro_display_SD_mult_PPM()
{
	if(Global_macro_need_refresh == PPM_AUTO)
	{
		Global_macro_need_refresh = NOREFRESH; // stop refresh
	}else{
		if( Global_data1 > 0)Global_SD_bitmap_folderNb=Global_data1;
		if( Global_data2 > 0)Global_SD_bitmap_fileNb=Global_data2;
		if( Global_data3 > 0)Global_SD_bitmap_speed=Global_data3;
		
		if(get_SD_bitmap_raw(Global_SD_bitmap_folderNb, Global_SD_bitmap_fileNb, 0, 0))
		{
			Global_macro_need_refresh = PPM_AUTO;
			Global_TS_beat8_start=GET_MILLIS();
		}
		
	}
}

void macro_display_SD_anim_PPM()
{
	if(Global_macro_need_refresh == ANIM)
	{
		Global_macro_need_refresh = NOREFRESH; // stop refresh
	}else{
		//if( Global_data1 > 0)Global_SD_bitmap_folderNb=Global_data1;
		if( Global_data1 > 0)Global_SD_bitmap_fileNb=Global_data2;
		if( Global_data2 > 0)Global_SD_bitmap_speed=Global_data3;
		
		if(get_SD_bitmap_anim(Global_SD_bitmap_folderNb, Global_SD_bitmap_fileNb, 0, 0))
		{
			Global_macro_need_refresh = ANIM;
			Global_TS_beat8_start=GET_MILLIS();
			Global_SD_Anim_pos=0;
		}
		
	}
}

void macro_glitter()
{
	
	if(Global_macro_need_refresh == GLITTER)
	{
		Global_macro_need_refresh = NOREFRESH; // stop refresh
	}else{
		Global_macro_need_refresh = GLITTER;
		if( Global_data1 > 0)Global_glitter_amount=Global_data1;
		if( Global_data2 > 0)Global_glitter_speed=Global_data2;
		if( Global_data3 > 0 && Global_data3 < ARRAY_SIZE(colors_table) )Global_glitter_color=Global_data3;
	}
}


//****    refresh Macros routines  *******************************
void refreshMacroStrobe()
{
	uint8_t beat = beat8( Global_strobe_speed, 0 );
 
	if ( beat >= 127 )
	{
		matrix.fillScreen(Global_strobe_color);
	}
	  
}

void refreshMacroFadeInOut()
{
	uint8_t beat=beat8( Global_fade_speed, Global_TS_beat8_start  );
	uint8_t wave=quadwave8(beat);
	matrix.setBrightness(fadeToBlackBy( Global_brightness, wave ));
}

void refreshMacroFadeIn()
{
	uint8_t beat=onebeat8( Global_fade_speed, Global_TS_beat8_start );
	matrix.setBrightness(fadeToBlackBy( Global_brightness, beat ));
}

void refreshMacroFadeOUT()
{
	uint8_t beat=onebeat8( Global_fade_speed, Global_TS_beat8_start  );
	matrix.setBrightness(brightToFullBy( Global_brightness, beat ));
}

void refreshMacroDisplayPPM()
{
	matrix.drawRGBBitmap(0,0,canvas.getBuffer(), SCREEN_MAX_X, SCREEN_MAX_Y);
}

void refreshMacroDisplayMultPPM()
{
	uint8_t beat=onebeat8( Global_SD_bitmap_speed, Global_TS_beat8_start );
	matrix.drawRGBBitmap(0,0,canvas.getBuffer(), SCREEN_MAX_X, SCREEN_MAX_Y);
	
	if(beat == 255)
	{
		Global_TS_beat8_start=GET_MILLIS();//reset timer
		while( ! get_SD_bitmap_raw(Global_SD_bitmap_folderNb, Global_SD_bitmap_fileNb++, 0, 0))
		{
			Global_SD_bitmap_fileNb=1;
		}
		
	}
	
}

void refreshMacroDisplayAnimPPM()
{
	//uint8_t beat=onebeat8( Global_SD_bitmap_speed, Global_TS_beat8_start );
	uint16_t pos = (SCREEN_MAX_X * SCREEN_MAX_Y * Global_SD_Anim_pos);
	//matrix.drawRGBBitmap(0,0,canvasX10.getBuffer(), SCREEN_MAX_X, SCREEN_MAX_Y);
	matrix.drawRGBBitmap(0,0,(canvasX10.getBuffer()) + pos, SCREEN_MAX_X, SCREEN_MAX_Y);
	
	//if(beat == 255)
	if(((GET_MILLIS()) - Global_TS_beat8_start) >= Global_SD_bitmap_speed)
	{
		Global_TS_beat8_start=GET_MILLIS();//reset timer
		Global_SD_Anim_pos++;
		if(Global_SD_Anim_pos >= 10)Global_SD_Anim_pos=0;
	}
	
}

void refreshMacroGlitter()
{
	int16_t x,y,color;
	if( Global_elapsed_glitter < Global_glitter_speed)return;
	Global_elapsed_glitter = 0; //reset timer
	color=Global_glitter_color;
	for(int i=0; i < Global_glitter_amount; i++)
	{
		x=random(SCREEN_MAX_X);
		y=random(SCREEN_MAX_Y);
		if(Global_glitter_color < 1)color=random(65535);
		matrix.drawPixel(x, y, color);
	}
}
//call by main refresh
boolean refresh_scrolling_text()
{
	//if(! Global_SD_file) return false; // no SD 
	if( Global_next_text_lenght < 1)return false; // no text to display
	
	// check if it's time to refresh scrolling
	if(Global_elapsed_scroll_time < Global_scroll_speed)return true; 
	
	Global_elapsed_scroll_time = 0;
	
	char strbuffer[DISPLAY_TEXT_LENGHT + 1];
	
	//if new text ready flag
	if(Global_flag_next_text_ready)
	{
		Global_flag_next_text_ready=0; //reset flag
		Global_scroll_char_start=0;
		if(Global_scroll_speed < 1) //means no scroll
		{
			Global_scroll_x = 0;
		}else{
			Global_scroll_x = matrix.width();
		}
	}
	
	strncpy ( strbuffer, Global_next_text_str + Global_scroll_char_start, sizeof(strbuffer)-1 );
	
	//deal with screen
	matrix.setBrightness(Global_brightness);
	matrix.fillScreen(0); //erase
	matrix.fillScreen(Global_background_color);
	matrix.setTextColor(Global_text_color);
	lRefreshMacros[Global_macro_need_refresh]();
	
	//deal with txt
	matrix.setCursor(Global_scroll_x, 0);
	matrix.print(strbuffer);
	//matrix.show();
	
	//prepare next round
	if( Global_scroll_speed > 0 && --Global_scroll_x <= (0 - DISPLAY_CHAR_WIDTH))
	{
		
		Global_scroll_char_start++;
		
		//check for the end of string
		if(Global_scroll_char_start > Global_next_text_lenght)
		{
			if(Global_flag_auto_next)
			{
				if(Global_flag_auto_next_elapsed > Global_flag_auto_next_wait)
				parse_display_next_text();
			}
			
			if(Global_flag_loop)
			{
				
				Global_scroll_char_start = 0;
				Global_scroll_x = matrix.width();
			}
			else
			{
				Global_scroll_x = 0 - DISPLAY_CHAR_WIDTH;
				Global_scroll_char_start = Global_next_text_lenght;
			}
		}else{
			
			Global_scroll_x = 0;
			
		}
	}
	return true; 
}


//****    refreshed as fast as possible  *******************************
// refresh matrix main
void refresh_matrix()
{
	if(Global_elapsed_matrix_refresh < SCREEN_REFRESH_RATE)return; 
	//led ON on lost frame (we're too slow !!!)
	digitalWriteFast(LED_BUILTIN,(Global_elapsed_matrix_refresh > (2*SCREEN_REFRESH_RATE))); 

	Global_elapsed_matrix_refresh=0; //reset timer
	
	if(! refresh_scrolling_text())
	{
		matrix.setBrightness(Global_brightness);
		matrix.fillScreen(0); //erase
		matrix.fillScreen(Global_background_color);
		lRefreshMacros[Global_macro_need_refresh]();
	}
	
	matrix.show();
}


// ******** MIDI stuffs ************************************************

void handleNoteOn(byte channel, byte number, byte value)
{
	if(number < ARRAY_SIZE(lMacros))lMacros[number]();
}

void handleNoteOff(byte channel, byte number, byte value)
{
	;
}

void handleControlChange(byte channel, byte number, byte value)
{
	if(number < ARRAY_SIZE(lControlChange))lControlChange[number](value);
}

void cc_set_text_file(byte value) //choose file number
{
	get_text_file(value);
}

void cc_set_brightness(byte value)
{
	Global_brightness=dim8_raw(value*2);
}

void cc_set_background_color(byte value)
{
	if(value < ARRAY_SIZE(colors_table))Global_background_color=colors_table[value];
}

void cc_set_text_color(byte value)
{
	if(value < ARRAY_SIZE(colors_table))Global_text_color=colors_table[value];
}

void cc_set_text_speed(byte value)
{
	Global_scroll_speed=value;
}

void cc_set_text_loop(byte value)
{
	if(value > 64)
	{
		Global_flag_loop=true;
	}else{
		Global_flag_loop=false;
	}
}

void cc_set_strobe_color(byte value)
{
	if(value < ARRAY_SIZE(colors_table))Global_strobe_color=colors_table[value];
}

void cc_set_strobe_speed(byte value) //value * 4 in BPM
{
	Global_strobe_speed=value*4;
}

void cc_set_fade_speed(byte value) //value * 4 in BPM
{
	Global_fade_speed=value*4;
}

void cc_set_bitmap_folder(byte value)
{
	Global_SD_bitmap_folderNb=value;
}

void cc_set_bitmap_file(byte value)
{
	Global_SD_bitmap_fileNb=value;
}

void cc_set_bitmap_speed(byte value)
{
	Global_SD_bitmap_speed=value; // in ms
}

void cc_set_glitter_amount(byte value)
{
	Global_glitter_amount=value*10; // nb of pixels 
}

void cc_set_glitter_speed(byte value)
{
	Global_glitter_speed=map(value,0,127,SCREEN_REFRESH_RATE,1000); //in ms
}

void cc_set_glitter_color(byte value)
{
	if(value < ARRAY_SIZE(colors_table))Global_glitter_color=colors_table[value];
}


//Serial Stuffs*********************************************************
// serial command ***************************
void setup_sCmd()
{
  // Setup callbacks for SerialCommand commands
  
  sCmd.addCommand("C", s_handle_ControlChange); //Control Change
  sCmd.addCommand("N", s_handle_NoteOn);	//Note On
  sCmd.addCommand("T", s_handle_Texte);	//enter text
  
  sCmd.setDefaultHandler(unrecognized); // Handler for command that isn't matched  (says "What?")
}

void unrecognized(const char *command)
{
  Serial.println("");
  Serial.print("erreur: ");
  Serial.println( command );
  Serial.println("");
  Serial.println("--------------------------------");
  Serial.println("");
  Serial.println(" \"C c n v\" Test send MidiContolChange: C channel ( 1 - 16)  CCnb ( 0 - 127 )  value ( 0 - 127 )  ; example:  C 0 12 127");
  Serial.println(" \"N c n v\" Test send MidiNoteOn: N channel ( 1 - 16)  note ( 0 - 127 )  velocity ( 0 - 127 )  ; example:  N 0 12 127");
  Serial.println("");
  Serial.println("--------------------------------");
  Serial.println("");
  return;
}

void s_handle_NoteOn()
{
  int aNum;
  char *arg;
  uint8_t	chan, note, vel;
  arg = sCmd.next();
  if (arg != NULL)
  {
    aNum = constrain(atoi(arg), 1, 16); // Converts a char string to an integer
  	chan = aNum;
  } else {
    Serial.println("error: need 3 arguments, N channel note velocity (example: N 1 15 67 )");
    return;
  }
  
  arg = sCmd.next();
  if (arg != NULL)
  {
	aNum = constrain(atoi(arg), 0, 127); // Converts a char string to an integer
	note = aNum;
  } else {
    Serial.println("error: need 3 arguments, N channel note velocity (example:  N 0 15 67 )");
    return;
  }
  
  arg = sCmd.next();
  if (arg != NULL)
  {
    aNum = constrain(atoi(arg), 0, 127); // Converts a char string to an integer
	vel = aNum;
  } else {
    Serial.println("error: need 3 arguments, N channel note velocity (example:  N 0 15 67 )");
    return;
  }
  
  Serial.print("Test NoteOn : ");
  Serial.print("channel ");  Serial.print(chan);
  Serial.print("  note ");  Serial.print(note);
  Serial.print("  velocity ");  Serial.println(vel);
  
  handleNoteOn(chan, note, vel);
  
}

void s_handle_ControlChange()
{
  int aNum;
  char *arg;
  uint8_t	chan, note, vel;
  arg = sCmd.next();
  if (arg != NULL)
  {
    aNum = constrain(atoi(arg), 1, 16); // Converts a char string to an integer
  	chan = aNum;
  } else {
    Serial.println("error: need 3 arguments, C channel CCnb value (example: C 1 15 67 )");
    return;
  }
  
  arg = sCmd.next();
  if (arg != NULL)
  {
	aNum = constrain(atoi(arg), 0, 127); // Converts a char string to an integer
	note = aNum;
  } else {
    Serial.println("error: need 3 arguments, C channel CCnb value (example: C 1 15 67 )");
    return;
  }
  
  arg = sCmd.next();
  if (arg != NULL)
  {
    aNum = constrain(atoi(arg), 0, 127); // Converts a char string to an integer
	vel = aNum;
  } else {
    Serial.println("error: need 3 arguments, C channel CCnb value (example: C 1 15 67 )");
    return;
  }
  
  Serial.print("Test ControlChange : ");
  Serial.print("channel ");  Serial.print(chan);
  Serial.print("  CCnb ");  Serial.print(note);
  Serial.print("  value ");  Serial.println(vel);
  
  handleControlChange(chan, note, vel);
  
}

void s_handle_Texte() 
{
  char *arg;
  char sBuffer[PARSER_TEXT_LEN];
  int res=0;
  sBuffer[0]=0;
  while(true)
  {
	arg = sCmd.next();
	if (arg == NULL)break;
	strcat ( sBuffer, arg );
	strcat ( sBuffer, " " );
  }
  res=snprintf(Global_next_text_str,PARSER_TEXT_LEN, "%s",sBuffer);
  if(res<0 || res>PARSER_TEXT_LEN)
  {
	Serial.println("Error! text invalid");
  }
  Global_next_text_lenght=res;
  Global_flag_next_text_ready=1;
  Global_data1=0; //reset for next macros
  Global_data2=0; //reset for next macros
  Global_data3=0; //reset for next macros
  Serial.print("New text received: ");
  Serial.println(Global_next_text_str);
}


//setup run once *******************************************************
void setup() 
{
	Serial.begin(115200);
	//delay(1000);
	
	Serial.println("**** 75x16 ws2812 leds matrix ****");
	Serial.println("Starting...");

	pinMode(LED_BUILTIN, OUTPUT);
	//init builtin SDcard
	init_SD();
	
	//init old fashion midi
	initMIDISerial();
	
	//init (USB) Serial commands (mostly fo debugging)
	setup_sCmd();
	
	//init led matrix
	matrix.begin();
	matrix.setTextWrap(false);
	matrix.setBrightness(MAX_BRIGHTNESS);
	matrix.setTextSize(2);
	matrix.setTextColor(colors_table[22]);
	
	Serial.println("Init all done !");
}


// loop run forever ***************************************************
void loop()
{
	MIDI.read();
	sCmd.readSerial();
	refresh_matrix();
}


//EndOfFile*************************************************************
