# particle-dht22

> Use a DHT22 temperature & humidity module w/ Particle Argon/Xenon/Boron

Yes, this again.

## Particle Cloud Functions

- `enable()` - Turns reading from module "on"
- `disable()` - Turns reading from module "off"
- `enableDeepSleep()` - Enables sleep, but not actually "deep" sleep
- `disableDeepSleep()` - Enables sleep, but not actually "deep" sleep
- `setDelay(short)` - Set update delay in ms

> Note: Particle Cloud Functions accept a single `String` value; this code coerces any args into the proper data type.

## Particle Cloud Variables

- `settings` - `{String}`: All settings in a JSON object. Properties:
  - `enabled` - `{bool}` (default `true`): Enabled or not
  - `deepSleep` - `{bool}` (default `true`): Deep sleep enabled or not
  - `delay` - `{short}` (default `900000`; 30 minutes): Current delay setting

## Notes

- This code persists settings in EEPROM. Should I be doing this? I have no idea.
- Going to try to hook this up to a LiPo battery
- Might add a INA219 to check battery and/or an RTC module to do actual deep sleep.
- The enclosure for this project will probably be clingfilm.

## License

Copyright © 2019 Christopher Hiller. Licensed Apache-2.0
