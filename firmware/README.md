# Прошивка RP2040-Zero

PlatformIO-проект для Waveshare RP2040-Zero на Arduino-Pico.

## Подключение

- встроенный WS2812B: GPIO16;
- пассивный пьезоизлучатель: GPIO15 → 100–220 Ом → пьезо → GND.

## Сборка

```bash
pio run
```

UF2 обычно находится здесь:

```text
.pio/build/waveshare_rp2040_zero/firmware.uf2
```

## Ночной профиль

Windows-программа отправляет:

```text
N,active,dayVolumePct,nightVolumePct,nightBrightnessPct
```

Дневная и ночная громкость независимы. Ночная яркость применяется к постоянному цвету ENG/РУС и вспышке переключения. Оранжевый offline-индикатор остаётся фиксированной яркости.
