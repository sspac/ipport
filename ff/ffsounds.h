/* Written by tools/ff_sounds.py - do not edit by hand. FFSND_<key> = the id to give ffsound_play; a sound with several parts (7 samples each) has FFSND_<key>_PARTS = n and the parts are consecutive ids. */
#define FF_NSOUNDS_EXTRA 19
#define FFSND_ClothRip 842
#define FFSND_EpicPunch 843
#define FFSND_EpicTwinkle 844
#define FFSND_GravediggerDig 845
#define FFSND_MukCharge 846
#define FFSND_MukChargeUp 847
#define FFSND_MukLaugh 848
#define FFSND_MukVomit 849
#define FFSND_Ricochet 850
#define FFSND_RolyPolyRoll 851
#define FFSND_SplashLarge 852
#define FFSND_SplashLargePlonkless 853
#define FFSND_TapEnd 854
#define FFSND_TapHoney01 855
#define FFSND_TapHoney02 856
#define FFSND_TapLoop 857
#define FFSND_TapStart 858
#define FFSND_TapTap 859
#define FFSND_WingFlap 860
static const struct { int id; const char *files[7]; } FF_SND_X[] = {
    { 842, { "ClothRip1.wav" } },
    { 843, { "punch.wav" } },
    { 844, { "Epic Twinkle.wav" } },
    { 845, { "gravedigger_dig.wav" } },
    { 846, { "DroolerCharge01.wav", "DroolerCharge02.wav" } },
    { 847, { "DroolerChargeUp.wav" } },
    { 848, { "DroolerLaugh.wav" } },
    { 849, { "MukVomit01.wav", "MukVomit02.wav" } },
    { 850, { "Rico01.wav", "Rico02.wav", "Rico03.wav", "Rico04.wav" } },
    { 851, { "rolling.wav" } },
    { 852, { "LargeSplash01.wav", "LargeSplash02.wav", "LargeSplash03.wav", "LargeSplash04.wav" } },
    { 853, { "LargeSplash03.wav", "LargeSplash04.wav" } },
    { 854, { "tap_close.wav" } },
    { 855, { "MP01.wav" } },
    { 856, { "MP02.wav" } },
    { 857, { "tap_loop.wav" } },
    { 858, { "tap_open1.wav" } },
    { 859, { "tap_open2.wav" } },
    { 860, { "WingFlap1.wav", "WingFlap2.wav" } },
    { 0, { 0 } } };
/* parts of the multi-part sounds: first id, count (ffsound_play picks one at random) */
static const struct { int first, n; } FF_SND_PARTS[] = {
    { 0, 0 } };
