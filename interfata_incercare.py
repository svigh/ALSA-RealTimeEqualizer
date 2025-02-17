import pygame
import os
from time import sleep
import RPi.GPIO as GPIO

button_map = {23:(255,0,0), 22:(0,255,0), 27:(0,0,255), 18:(0,0,0)}

GPIO.setmode(GPIO.BCM)
for k in button_map.keys():
        GPIO.setup(k, GPIO.IN, pull_up_down=GPIO.PUD_UP)

WHITE = (255,255,255)

os.putenv('SDL_FBDEV', '/dev/fb1')
pygame.init()
pygame.mouse.set.visible(False)
lcd = pygame.display.set_mode((320, 240))
lcd.fill((0,0,0))
pygame.display.update()

font_big = pygame.font.Font(None,100)

while True:
        #Scan the buttons
        for (k,v) in button_map.items():
                if GPIO.input(k) == False:
                        lcd.fill(v)
                        text_surface = font_big.render('%d %k', True, WHITE)
                        rect =  text_surface.get_rect(center=(160,120))
                        lcd.bilt(text_surface, rect)
                        pygame.display.update()
                sleep(0.1)
                                                       
