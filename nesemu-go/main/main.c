#include <freertos/FreeRTOS.h>
#include "odroid_system.h"

#include <string.h>
#include <stdint.h>
#include <noftypes.h>
#include <bitmap.h>
#include <event.h>
#include <nes.h>
#include <nes_input.h>
#include <nes_state.h>
#include <nes_input.h>
#include <osd.h>
#include "sdkconfig.h"

#define APP_ID 10

#define AUDIO_SAMPLE_RATE   32000

#define NVS_KEY_LIMIT_SPRITES "limitspr"
#define NVS_KEY_OVERSCAN "overscan"

static char* romData;
static size_t romSize;

static uint16_t myPalette[64];
static odroid_video_frame update1 = {NES_SCREEN_WIDTH, NES_SCREEN_HEIGHT, 0, 1, 0x3F, -1, NULL, myPalette, 0};
static odroid_video_frame update2 = {NES_SCREEN_WIDTH, NES_SCREEN_HEIGHT, 0, 1, 0x3F, -1, NULL, myPalette, 0};
static odroid_video_frame *currentUpdate = &update1;

static void (*audio_callback)(void *buffer, int length) = NULL;
static int16_t *audioBuffer;

static odroid_gamepad_state joystick1;
static odroid_gamepad_state joystick2;
static odroid_gamepad_state *localJoystick = &joystick1;
static odroid_gamepad_state *remoteJoystick = &joystick2;

static uint overscan = 0;
static bool netplay = false;

bool fullFrame = 0;
// --- MAIN


static void netplay_callback(netplay_event_t event, void *arg)
{
   bool new_netplay;

   switch (event)
   {
      case NETPLAY_EVENT_STATUS_CHANGED:
         new_netplay = (odroid_netplay_status() == NETPLAY_STATUS_CONNECTED);

         if (netplay && !new_netplay)
         {
            odroid_overlay_alert("Connection lost!");
         }
         else if (!netplay && new_netplay)
         {
            displayScalingMode = ODROID_DISPLAY_SCALING_FILL;
            displayFilterMode = ODROID_DISPLAY_FILTER_NONE;
            forceVideoRefresh = true;
            nes_reset(ZERO_RESET);
         }

         netplay = new_netplay;
         break;

      default:
         break;
   }

   if (netplay && odroid_netplay_mode() == NETPLAY_MODE_GUEST)
   {
      localJoystick = &joystick2;
      remoteJoystick = &joystick1;
   }
   else
   {
      localJoystick = &joystick1;
      remoteJoystick = &joystick2;
   }
}


static bool SaveState(char *pathName)
{
   return state_save(pathName) >= 0;
}

static bool LoadState(char *pathName)
{
   if (state_load(pathName) < 0)
   {
      nes_reset(HARD_RESET);
      return false;
   }
   return true;
}


static void set_overscan(bool enabled)
{
   overscan = enabled ? nes_getptr()->overscan : 0;
   update1.height = update2.height = NES_SCREEN_HEIGHT - (overscan * 2);
}

static bool sprite_limit_cb(odroid_dialog_choice_t *option, odroid_dialog_event_t event)
{
   int val = odroid_settings_int32_get(NVS_KEY_LIMIT_SPRITES, 1);

   if (event == ODROID_DIALOG_PREV || event == ODROID_DIALOG_NEXT) {
      val = val ? 0 : 1;
      odroid_settings_int32_set(NVS_KEY_LIMIT_SPRITES, val);
      ppu_limitsprites(val);
   }

   strcpy(option->value, val ? "On " : "Off");

   return event == ODROID_DIALOG_ENTER;
}

static bool overscan_update_cb(odroid_dialog_choice_t *option, odroid_dialog_event_t event)
{
   int val = odroid_settings_app_int32_get(NVS_KEY_OVERSCAN, 1);

   if (event == ODROID_DIALOG_PREV || event == ODROID_DIALOG_NEXT) {
      val = val ? 0 : 1;
      odroid_settings_app_int32_set(NVS_KEY_OVERSCAN, val);
      set_overscan(val);
   }

   strcpy(option->value, val ? "Auto" : "Off ");

   return event == ODROID_DIALOG_ENTER;
}

static bool region_update_cb(odroid_dialog_choice_t *option, odroid_dialog_event_t event)
{
   int val = odroid_settings_Region_get();
   int max = 2;

   if (event == ODROID_DIALOG_PREV) val = val > 0 ? val - 1 : max;
   if (event == ODROID_DIALOG_NEXT) val = val < max ? val + 1 : 0;

   if (event == ODROID_DIALOG_PREV || event == ODROID_DIALOG_NEXT) {
      odroid_settings_Region_set(val);
   }

   if (val == ODROID_REGION_AUTO) strcpy(option->value, "Auto");
   if (val == ODROID_REGION_NTSC) strcpy(option->value, "NTSC");
   if (val == ODROID_REGION_PAL)  strcpy(option->value, "PAL ");

   return event == ODROID_DIALOG_ENTER;
}

static bool advanced_settings_cb(odroid_dialog_choice_t *option, odroid_dialog_event_t event)
{
   if (event == ODROID_DIALOG_ENTER) {
      odroid_dialog_choice_t options[] = {
         {1, "Region", "Auto", 1, &region_update_cb},
         {2, "Overscan", "Auto", 1, &overscan_update_cb},
         {3, "Sprite limit", "On ", 1, &sprite_limit_cb},
         // {4, "", "", 1, NULL},
         //{0, "Reset all", "", 1, NULL},
         ODROID_DIALOG_CHOICE_LAST
      };
      odroid_overlay_dialog("Advanced", options, 0);
   }
   return false;
}

static bool palette_update_cb(odroid_dialog_choice_t *option, odroid_dialog_event_t event)
{
   int pal = odroid_settings_Palette_get();
   int max = PPU_PAL_COUNT - 1;

   if (event == ODROID_DIALOG_PREV) pal = pal > 0 ? pal - 1 : max;
   if (event == ODROID_DIALOG_NEXT) pal = pal < max ? pal + 1 : 0;

   if (event == ODROID_DIALOG_PREV || event == ODROID_DIALOG_NEXT) {
      odroid_settings_Palette_set(pal);
      ppu_setnpal(pal);
      odroid_display_queue_update(currentUpdate, NULL);
      odroid_display_queue_update(currentUpdate, NULL);
   }

   sprintf(option->value, "%.7s", ppu_getnpal(pal)->name);
   return event == ODROID_DIALOG_ENTER;
}


void osd_fullname(char *fullname, const char *shortname)
{
   strncpy(fullname, shortname, PATH_MAX);
}

char *osd_newextension(char *string, char *ext)
{
   return string;
}

size_t osd_getromdata(unsigned char **data)
{
	*data = (unsigned char*)romData;
   return romSize;
}

void osd_loadstate()
{
   if (odroid_system_get_start_action() == ODROID_START_ACTION_RESUME)
   {
      odroid_system_emu_load_state(0);
   }

   ppu_limitsprites(odroid_settings_int32_get(NVS_KEY_LIMIT_SPRITES, 1));
   ppu_setnpal(odroid_settings_Palette_get());
   set_overscan(odroid_settings_app_int32_get(NVS_KEY_OVERSCAN, 1));
}

void osd_logprint(int type, char *string)
{
   printf("%s", string);
}

int osd_init()
{
   return 0;
}

void osd_shutdown()
{
	audio_callback = NULL;
}

/*
** Audio
*/
void osd_audioframe(int audioSamples)
{
   audio_callback(audioBuffer, audioSamples); //get audio data

   //16 bit mono -> 32-bit (16 bit r+l)
   for (int i = audioSamples - 1; i >= 0; --i)
   {
      int16_t sample = audioBuffer[i];
      audioBuffer[i*2] = sample;
      audioBuffer[i*2+1] = sample;
   }

   odroid_audio_submit(audioBuffer, audioSamples);
}

void osd_setsound(void (*playfunc)(void *buffer, int length))
{
   //Indicates we should call playfunc() to get more data.
   audio_callback = playfunc;
}

void osd_getsoundinfo(sndinfo_t *info)
{
   info->sample_rate = AUDIO_SAMPLE_RATE;
   info->bps = 16;
}

/*
** Video
*/
void osd_setpalette(rgb_t *pal)
{
   for (int i = 0; i < 64; i++)
   {
      uint16_t c = (pal[i].b>>3) + ((pal[i].g>>2)<<5) + ((pal[i].r>>3)<<11);
      myPalette[i] = (c>>8) | ((c&0xff)<<8);
   }
   forceVideoRefresh = true;
}

void IRAM_ATTR osd_blitscreen(bitmap_t *bmp)
{
   odroid_video_frame *previousUpdate = (currentUpdate == &update1) ? &update2 : &update1;

   currentUpdate->buffer = bmp->line[overscan];
   currentUpdate->stride = bmp->pitch;

   fullFrame = odroid_display_queue_update(currentUpdate, previousUpdate) == SCREEN_UPDATE_FULL;

   currentUpdate = previousUpdate;
}

void osd_getinput(void)
{
	static const int events[] = {
      event_joypad1_start, event_joypad1_select, event_joypad1_up, event_joypad1_right,
      event_joypad1_down, event_joypad1_left, event_joypad1_a, event_joypad1_b,
      event_joypad2_start, event_joypad2_select, event_joypad2_up, event_joypad2_right,
      event_joypad2_down, event_joypad2_left, event_joypad2_a, event_joypad2_b
	};
   static const int events_count = sizeof(events) / sizeof(int);

   static uint16 previous = 0xffff;
   uint16 b = 0, changed = 0;

   odroid_input_gamepad_read(localJoystick);

   if (localJoystick->values[ODROID_INPUT_MENU]) {
      odroid_overlay_game_menu();
   }
   else if (localJoystick->values[ODROID_INPUT_VOLUME]) {
      odroid_dialog_choice_t options[] = {
            {100, "Palette", "Default", 1, &palette_update_cb},
            {101, "More...", "", 1, &advanced_settings_cb},
            ODROID_DIALOG_CHOICE_LAST
      };
      odroid_overlay_game_settings_menu(options);
   }

   if (netplay) {
      odroid_netplay_sync(localJoystick, remoteJoystick, sizeof(odroid_gamepad_state));
   }

	if (!joystick1.values[ODROID_INPUT_START])  b |= (1 << 0);
	if (!joystick1.values[ODROID_INPUT_SELECT]) b |= (1 << 1);
	if (!joystick1.values[ODROID_INPUT_UP])     b |= (1 << 2);
	if (!joystick1.values[ODROID_INPUT_RIGHT])  b |= (1 << 3);
	if (!joystick1.values[ODROID_INPUT_DOWN])   b |= (1 << 4);
	if (!joystick1.values[ODROID_INPUT_LEFT])   b |= (1 << 5);
	if (!joystick1.values[ODROID_INPUT_A])      b |= (1 << 6);
	if (!joystick1.values[ODROID_INPUT_B])      b |= (1 << 7);
	if (!joystick2.values[ODROID_INPUT_START])  b |= (1 << 8);
	if (!joystick2.values[ODROID_INPUT_SELECT]) b |= (1 << 9);
	if (!joystick2.values[ODROID_INPUT_UP])     b |= (1 << 10);
	if (!joystick2.values[ODROID_INPUT_RIGHT])  b |= (1 << 11);
	if (!joystick2.values[ODROID_INPUT_DOWN])   b |= (1 << 12);
	if (!joystick2.values[ODROID_INPUT_LEFT])   b |= (1 << 13);
	if (!joystick2.values[ODROID_INPUT_A])      b |= (1 << 14);
	if (!joystick2.values[ODROID_INPUT_B])      b |= (1 << 15);

   changed = b ^ previous;
   previous = b;

	for (int x = 0; x < events_count; x++) {
		if (changed & 1) {
         event_raise(events[x], (b & 1) ? INP_STATE_BREAK : INP_STATE_MAKE);
		}
		changed >>= 1;
		b >>= 1;
	}
}


void app_main(void)
{
	printf("nesemu (%s-%s).\n", COMPILEDATE, GITREV);

   odroid_system_init(APP_ID, AUDIO_SAMPLE_RATE);
   odroid_system_emu_init(&LoadState, &SaveState, &netplay_callback);

   audioBuffer = rg_alloc(AUDIO_SAMPLE_RATE / 50 * 4, MEM_DMA);
   romData     = rg_alloc(1024 * 1024, MEM_ANY);

   char *romPath = odroid_system_get_path(NULL, ODROID_PATH_ROM_FILE);

   // Load ROM
   if (strcasecmp(romPath + (strlen(romPath) - 4), ".zip") == 0)
   {
      printf("app_main ROM: Reading compressed file: %s\n", romPath);
      romSize = odroid_sdcard_unzip_file_to_memory(romPath, romData, 1024 * 1024);
   }
   else
   {
      printf("app_main ROM: Reading file: %s\n", romPath);
      romSize = odroid_sdcard_copy_file_to_memory(romPath, romData, 1024 * 1024);
   }

   printf("app_main ROM: romSize=%d\n", romSize);
   if (romSize <= 0)
   {
      odroid_system_panic("ROM file loading failed!");
   }

   int region, ret;

   switch(odroid_settings_Region_get())
   {
      case ODROID_REGION_AUTO: region = NES_AUTO; break;
      case ODROID_REGION_NTSC: region = NES_NTSC; break;
      case ODROID_REGION_PAL:  region = NES_PAL;  break;
      default: region = NES_NTSC; break;
   }

   printf("Nofrendo start!\n");

   ret = nofrendo_start(romPath, region);

   switch (ret)
   {
      case -1: odroid_system_panic("Init failed.\n");
      case -2: odroid_system_panic("Unsupported ROM.\n");
      default: odroid_system_panic("Nofrendo died!\n");
   }
}
