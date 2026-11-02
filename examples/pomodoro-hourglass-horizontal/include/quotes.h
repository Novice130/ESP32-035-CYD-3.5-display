// ==============================================================================
//  quotes.h - Curated collection of inspirational wisdom for Pomodoro Timer
//  Ratio: ~10 Islamic quotes (Quran, Hadith, Companions, Scholars) to 1 General wisdom
// ==============================================================================
#pragma once
#include <stdint.h>
#include <stddef.h>

struct Quote {
    const char* text;
    const char* author;
};

static const Quote INSPIRATIONAL_QUOTES[] = {
    // --- 1. PERSEVERANCE & HARD WORK ---
    {"And that man can have nothing but what he strives for.", "Quran 53:39"},
    {"Those who strive for Us, We will surely guide them to Our ways.", "Quran 29:69"},
    {"The most beloved deeds to Allah are consistent, even if small.", "Sahih Bukhari"},
    {"Tie your camel first, then put your trust in Allah.", "Sunan al-Tirmidhi"},
    {"If the Final Hour comes while you hold a sapling, plant it.", "Musnad Ahmad"},
    {"Good actions are a guard against the blows of adversity.", "Abu Bakr as-Siddiq (RA)"},
    {"May Allah bless the one who says less and does more.", "Umar ibn al-Khattab (RA)"},
    {"The value of every person is in what they do well.", "Ali ibn Abi Talib (RA)"},
    {"Knowledge calls for action: if it answers, it stays; else it leaves.", "Ali ibn Abi Talib (RA)"},
    {"Knowledge without action is waste; action without knowledge is folly.", "Imam al-Ghazali"},
    {"Patience with small details makes perfect a large work.", "Mevlana Rumi"},
    {"The secret of getting ahead is getting started.", "Mark Twain"}, // General Wisdom #1

    // --- 2. PATIENCE (SABR) & TRUST (TAWAKKUL) ---
    {"Verily, with hardship comes ease.", "Quran 94:6"},
    {"Indeed, Allah is with those who patiently endure.", "Quran 2:153"},
    {"Whoever relies upon Allah, then He is sufficient for him.", "Quran 65:3"},
    {"Allah does not burden a soul beyond that it can bear.", "Quran 2:286"},
    {"Amazing is the believer's affair; all of it is good for him.", "Sahih Muslim"},
    {"Know that victory comes with patience, and relief with hardship.", "Sunan al-Tirmidhi"},
    {"True patience is shown at the first blow of adversity.", "Sahih Bukhari"},
    {"There is no trouble in patience, and no benefit in complaining.", "Abu Bakr as-Siddiq (RA)"},
    {"We found the best part of our lives through patience.", "Umar ibn al-Khattab (RA)"},
    {"Patience is to faith what the head is to the body.", "Ali ibn Abi Talib (RA)"},
    {"To get what you love, you must first be patient with what you hate.", "Imam al-Ghazali"},
    {"The wound is the place where the Light enters you.", "Mevlana Rumi"},
    {"You have power over your mind, not outside events. Realize this.", "Marcus Aurelius"}, // General Wisdom #2

    // --- 3. TIME, FOCUS & MINDFULNESS ---
    {"By time, mankind is in loss, except those who believe and do good.", "Quran 103:1-3"},
    {"So when you have finished your duties, still labor hard.", "Quran 94:7"},
    {"Allah will not change a people until they change what is in themselves.", "Quran 13:11"},
    {"A sign of good Islam is leaving what does not concern you.", "Sunan al-Tirmidhi"},
    {"Worship Allah as though you see Him; for if you see Him not, He sees you.", "Sahih Bukhari"},
    {"Take benefit of youth before old age, and health before sickness.", "Al-Hakim"},
    {"Be mindful of Allah, and you will find Him before you.", "Sunan al-Tirmidhi"},
    {"Call yourselves to account before you are called to account.", "Umar ibn al-Khattab (RA)"},
    {"Do not put off today's work until tomorrow.", "Umar ibn al-Khattab (RA)"},
    {"You are but a collection of days; when a day passes, a part of you goes.", "Hasan al-Basri"},
    {"Every breath of yours is a priceless jewel, for it is irreplaceable.", "Imam al-Ghazali"},
    {"Silence is the language of God; all else is a poor translation.", "Mevlana Rumi"},
    {"It is not that we have a short time to live, but that we waste much of it.", "Seneca"}, // General Wisdom #3

    // --- 4. DEEP REFLECTIONS & WISDOM ---
    {"Do not grieve. Anything you lose comes round in another form.", "Mevlana Rumi"},
    {"Desires make slaves out of kings; patience makes kings out of slaves.", "Imam al-Ghazali"},
    {"He who is not saved by patience is destroyed by panic.", "Ali ibn Abi Talib (RA)"},
    {"First say to yourself what you would be, and then do what you must do.", "Epictetus"} // General Wisdom #4
};

static const int NUM_QUOTES = sizeof(INSPIRATIONAL_QUOTES) / sizeof(INSPIRATIONAL_QUOTES[0]);

// Milestone praise strings for 10% through 100%
static const char* const MILESTONE_PRAISES[] = {
    "Starting strong! Focus is the first step.",
    "10% reached! Great momentum, keep going.",
    "20% complete! In the deep work flow.",
    "30% accomplished! Moving forward steadily.",
    "40% done! Discipline is yielding fruit.",
    "50% halfway! You are mastering your time.",
    "60% achieved! Outstanding concentration.",
    "70% conquered! The summit is in view.",
    "80% finished! Excellent perseverance.",
    "90% final stretch! Finish with excellence.",
    "100% complete! Masha'Allah, marvelous job!"
};
