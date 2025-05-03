# https://github.com/raspberrypi/picamera2/tree/main/examples

import cv2
import time
from picamera2 import Picamera2
import serial

# sets up the serial link to the Arduino mega using gpio pin 14
arduino = serial.Serial('/dev/serial0', 115200, timeout=1)
# adds a sleep timer for the arduino to reset
time.sleep(2)

# sets up the camera for facial tracking using haarcascades
face_detector = cv2.CascadeClassifier("/usr/share/opencv4/haarcascades/haarcascade_frontalface_default.xml")
cv2.startWindowThread()

# creates a picamera object called picam2
picam2 = Picamera2()
# configures the camera
picam2.configure(picam2.create_preview_configuration(main={"format": 'XRGB8888', "size": (640, 480)}))
# calls the start method for picam2
picam2.start()

# creates a loop for continuos facial tracking
while True:
    # sets the physcal parameters for the camera feed
    frame_width = 680
    frame_height = 480
    # sets the ideal headsize for distance tracking (scaling for distance)
    headsize = 150
    center_x = frame_width / 2
    center_y = frame_height / 2

    # tells the picture to take pictures
    im = picam2.capture_array()
    # converts the pictures to greyscale
    grey = cv2.cvtColor(im, cv2.COLOR_BGR2GRAY)
    # checks the image for faces
    faces = face_detector.detectMultiScale(grey, 1.1, 5)
    # creates a boolean for face_found that is normally false
    face_found = False

    # creates a function to scale the pwm values
    def scale_pwm(val):
        return 1500 + int(val * (500 / 255))

    # creates a for loop that runs through the camera values
    for (x, y, w, h) in faces:
        # for when the face is found, this is set to true
        face_found = True

        # pastes a rectangle over the face
        cv2.rectangle(im, (x, y), (x + w, y + h), (0, 255, 0))
        # pastes a dot over the center of the face for troubleshooting purposes
        cv2.circle(im, (int(x + w / 2), int(y + h / 2)), 5, (255, 0, 0))

        # finds the average size of the head rectangle
        headtangle = (w + h) / 2
        # sets the x postion as the center point
        x_position = int(center_x - (x + w / 2))
        # sets the y position as the center point
        y_position = int(center_y - (y + h / 2))
        # sets the distance from the person based on the size of their head relative to the window
        distance = headsize - headtangle

        # scales the values between -255 to 255
        scaled_x = max(min(int(255 * (x_position / center_x)), 255), -255)
        scaled_y = max(min(int(255 * (y_position / center_y)), 255), -255)
        scaled_head = max(min(int(255 * (distance / headsize)), 255), -255)

        # creates pwm signals to send to the arduino
        pwm1 = scale_pwm(scaled_head)
        pwm2 = scale_pwm(scaled_x)
        pwm3 = scale_pwm(scaled_y)

        # creates a command f string that sends all 3 signals as comma seperated values
        command = f"{pwm1},{pwm2},{pwm3}\n"
        # writes to the arduino an encoded command string over serial
        arduino.write(command.encode())
        # prints that a face was found and the commands that go with them
        print("Face found - sent:", command.strip())

    # if no face was found, the sent pwm signal is neutral (does nothing)
    if not face_found:
        neutral_command = "1500,1500,1500\n"
        arduino.write(neutral_command.encode())
        print("No face - robot stopped.")

    # pulls up a camera window and uses waitkey to keep it open
    cv2.imshow("Camera", im)
    cv2.waitKey(1)
