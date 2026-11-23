/* Written by tools/ff_sounds.py - do not edit by hand. FFSND_<key> = the id to give ffsound_play; a sound with several parts (7 samples each) has FFSND_<key>_PARTS = n and the parts are consecutive ids. */
#define FF_NSOUNDS_EXTRA 46
#define FFSND_BaloonShort 842
#define FFSND_BeeBuzz 843
#define FFSND_BeeBuzzDown 844
#define FFSND_BeeBuzzPrep 845
#define FFSND_ClothRip 846
#define FFSND_CobWeb 847
#define FFSND_CobWeb_PARTS 2
#define FFSND_CoughRasp 849
#define FFSND_DevilDaggerGemCollect 850
#define FFSND_EpicPunch 851
#define FFSND_EpicTwinkle 852
#define FFSND_FireFizzle 853
#define FFSND_FireLight 854
#define FFSND_FlashShakeyKidRoar 855
#define FFSND_FlashZap 856
#define FFSND_Foresee 857
#define FFSND_ForeseerClap 858
#define FFSND_FrayedAlert 859
#define FFSND_FriedEnd 860
#define FFSND_FriedLoop 861
#define FFSND_FriedStart 862
#define FFSND_FrogShoot 863
#define FFSND_GolemDoorClose 864
#define FFSND_GravediggerDig 865
#define FFSND_Lighter 866
#define FFSND_MonsterYellFlash 867
#define FFSND_MukCharge 868
#define FFSND_MukChargeUp 869
#define FFSND_MukLaugh 870
#define FFSND_MukVomit 871
#define FFSND_Plorp 872
#define FFSND_Ricochet 873
#define FFSND_RolyPolyRoll 874
#define FFSND_RubberRockBounce 875
#define FFSND_SmashHitWeak 876
#define FFSND_SplashLarge 877
#define FFSND_SplashLargePlonkless 878
#define FFSND_TapEnd 879
#define FFSND_TapHoney01 880
#define FFSND_TapHoney02 881
#define FFSND_TapLoop 882
#define FFSND_TapStart 883
#define FFSND_TapTap 884
#define FFSND_Waterboom 885
#define FFSND_WingFlap 886
#define FFSND_ZoomAway 887
static const struct { int id; const char *files[7]; } FF_SND_X[] = {
    { 842, { "BalloonBlowingUpShort.wav" } },
    { 843, { "beebuzz.wav" } },
    { 844, { "beebuzzDown.wav" } },
    { 845, { "beebuzzPrep.wav" } },
    { 846, { "ClothRip1.wav" } },
    { 847, { "Cobweb_break1.wav", "Cobweb_break2.wav", "Cobweb_break3.wav", "Cobweb_break4.wav", "Cobweb_break5.wav", "Cobweb_break6.wav", "Cobweb_step1.wav" } },
    { 848, { "Cobweb_step2.wav", "Cobweb_step3.wav", "Cobweb_step4.wav", "Cobweb_step5.wav", "Cobweb_step6.wav" } },
    { 849, { "CoughRasp01.wav", "CoughRasp02.wav", "CoughRasp03.wav", "CoughRasp04.wav" } },
    { 850, { "DevilDaggerGemCollect01.wav", "DevilDaggerGemCollect02.wav", "DevilDaggerGemCollect03.wav", "DevilDaggerGemCollect04.wav" } },
    { 851, { "punch.wav" } },
    { 852, { "Epic Twinkle.wav" } },
    { 853, { "FireFizzle.wav" } },
    { 854, { "FireLight01.wav", "FireLight02.wav" } },
    { 855, { "ShakeyKidRoar0.wav", "ShakeyKidRoar1.wav", "ShakeyKidRoar2.wav" } },
    { 856, { "FlashZap01.wav", "FlashZap02.wav", "FlashZap03.wav" } },
    { 857, { "Foresee.wav" } },
    { 858, { "ForeseerClap.wav" } },
    { 859, { "AlertShort.wav" } },
    { 860, { "fire3.wav" } },
    { 861, { "fire2.wav" } },
    { 862, { "fire1.wav" } },
    { 863, { "frogshoot2.wav" } },
    { 864, { "golem_trapdoor_close0.wav", "golem_trapdoor_close1.wav" } },
    { 865, { "gravedigger_dig.wav" } },
    { 866, { "Lighter.wav" } },
    { 867, { "MonsterYellA0.wav" } },
    { 868, { "DroolerCharge01.wav", "DroolerCharge02.wav" } },
    { 869, { "DroolerChargeUp.wav" } },
    { 870, { "DroolerLaugh.wav" } },
    { 871, { "MukVomit01.wav", "MukVomit02.wav" } },
    { 872, { "plorp_1.wav", "plorp_2.wav", "plorp_3.wav", "plorp_4.wav" } },
    { 873, { "Rico01.wav", "Rico02.wav", "Rico03.wav", "Rico04.wav" } },
    { 874, { "rolling.wav" } },
    { 875, { "rubberrock_01.wav", "rubberrock_02.wav", "rubberrock_03.wav" } },
    { 876, { "SmashHitWeak.wav" } },
    { 877, { "LargeSplash01.wav", "LargeSplash02.wav", "LargeSplash03.wav", "LargeSplash04.wav" } },
    { 878, { "LargeSplash03.wav", "LargeSplash04.wav" } },
    { 879, { "tap_close.wav" } },
    { 880, { "MP01.wav" } },
    { 881, { "MP02.wav" } },
    { 882, { "tap_loop.wav" } },
    { 883, { "tap_open1.wav" } },
    { 884, { "tap_open2.wav" } },
    { 885, { "waterboom.wav" } },
    { 886, { "WingFlap1.wav", "WingFlap2.wav" } },
    { 887, { "zoom_away.wav" } },
    { 0, { 0 } } };
/* parts of the multi-part sounds: first id, count (ffsound_play picks one at random) */
static const struct { int first, n; } FF_SND_PARTS[] = {
    { 847, 2 },
    { 0, 0 } };
