# water_my_garden_cpp
will RIIR later, now my garden dries out, need to do it fast!
![285891608_3337123606517733_815334365603212199_n](https://user-images.githubusercontent.com/1136779/172473788-cb709ec7-fafa-4fea-adef-954032477c84.jpg)
![286076194_2646548522155910_2422992089338040563_n](https://user-images.githubusercontent.com/1136779/172473802-e5e4e7a6-0cc4-49eb-9eca-fc8e8e3cd3b3.jpg)


## PINOUT
https://randomnerdtutorials.com/esp32-pinout-reference-gpios/

https://i0.wp.com/randomnerdtutorials.com/wp-content/uploads/2018/08/ESP32-DOIT-DEVKIT-V1-Board-Pinout-36-GPIOs-updated.jpg?quality=100&strip=all&ssl=1

https://deepbluembedded.com/esp32-adc-tutorial-read-analog-voltage-arduino/


# Status
[ x ] Connect relay switch and make it work

[ x ] Connect moisture sensor and make it work

[ x ] Connect RTC and make it work

[ x ] RTC: sync time

[ x ] RTC: use interrupt to wakeup schedule

[ x ] Make communication between tasks

[ x ] add schedule

[ x ] Convert 12V AC to 5V DC

[ x ] Assemble in the case

[ x ] Enable wifi

[ ] Start webserver to control

    [ x ] Q&D POC

    [ ] authorization

    [ ] cool REST API

    [ ] https

    [ ] cleanup webserver code

# moisture sensor
https://www.sigmaelectronica.net/wp-content/uploads/2018/04/sen0193-humedad-de-suelos.pdf

# rtc
RTC handling taken from:
https://github.com/nopnop2002/esp-idf-ds3231

