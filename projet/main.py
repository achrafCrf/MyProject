import Adafruit_BBIO.GPIO as GPIO
import time
from projet.capteur import setup, get_distance

LED_PIN = "P9_14"
DIST_THRESHOLD = 10  # cm

def main():
    GPIO.setup(LED_PIN, GPIO.OUT)
    setup()
    try:
        while True:
            dist = get_distance()
            print(f"Distance: {dist} cm")
            if dist < DIST_THRESHOLD:
                GPIO.output(LED_PIN, GPIO.HIGH)
            else:
                GPIO.output(LED_PIN, GPIO.LOW)
            time.sleep(0.5)
    except KeyboardInterrupt:
        GPIO.cleanup()

if __name__ == "__main__":
    main()
