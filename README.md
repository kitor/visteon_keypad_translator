# visteon_keypad_translator
Arduino code for swapping keypads between Visteon Stereo from Fiat Stilo and Bravo

Note: unfortunately Bravo uses 3.3v logic while Stilo has 5v. All 3.3v Pro Micro clones I ordered have design bug - they run from 5v, skipping stabilizer, and won't work when 'fixed' to 3.3v. Thus it was never tested in Bravo + Stilo frontpanel configuration, only opposite.
