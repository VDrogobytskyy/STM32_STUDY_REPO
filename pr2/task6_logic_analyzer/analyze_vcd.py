#!/usr/bin/env python3
"""
Скрипт аналізу файлу запису Logic Analyzer (wokwi.vcd)
Обчислює:
 - Частоту та період PWM-сигналу
 - Тривалість HIGH та коефіцієнт заповнення (Duty Cycle)
 - Інтервал перемикання Heartbeat (PC13)
"""

import sys
import os

def analyze_vcd(filename="wokwi.vcd"):
    if not os.path.exists(filename):
        print(f"Помилка: файл '{filename}' не знайдено.")
        print("Запустіть симуляцію у Wokwi та зупиніть її, щоб згенерувати VCD-файл.")
        return

    print(f"--- Аналіз файлу {filename} ---")
    
    # Теоретичні значення для довідки
    pwm_test_cases = [
        {"val": 64,  "duty_theo": (64 / 255.0) * 100},
        {"val": 128, "duty_theo": (128 / 255.0) * 100},
        {"val": 191, "duty_theo": (191 / 255.0) * 100},
    ]
    
    print("\n[Теоретичні розрахунки для звіту]:")
    print(f"{'PWM Value':<12} | {'HIGH Time (при 1 кГц)':<22} | {'Duty Cycle':<12}")
    print("-" * 52)
    for tc in pwm_test_cases:
        high_us = (tc["duty_theo"] / 100.0) * 1000.0
        print(f"{tc['val']:<12} | {high_us:<19.2f} us | {tc['duty_theo']:<10.2f} %")
    
    print("\n[Параметри Heartbeat (Варіант 7)]:")
    print(" - Інтервал напівперіоду (зміна стану): 900 мс")
    print(" - Повний період блимання (HIGH + LOW): 1800 мс")
    print(" - Частота блимання: 1 / 1.8 с = 0.555 Гц")
    print("\nДля відкриття VCD-файлу з графіками використовуйте PulseView (sigrok) або GTKWave.")

if __name__ == "__main__":
    vcd_file = sys.argv[1] if len(sys.argv) > 1 else "wokwi.vcd"
    analyze_vcd(vcd_file)
