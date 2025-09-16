# DC-Motor-Speed-Controller

This project is a closed-loop PI speed controller for a DC motor. It uses an MSP430 microcontroller to generate PWM signals and a custom-designed H-Bridge PCB to drive the motor. Varying loads were added to a second generator motor for different current draws and to monitor the PI controllers response.

### Project Showcase
![Photo of the final custom-designed PCB and full assembly](images/Final-Setup.jpg)


### Key Features
* **Control:** A software PI controller implemented in C precisely regulates motor speed to a desired setpoint that is input by the user.
* **Hardware:** A custom PCB for the H-Bridge power driver was designed in KiCad to handle high motor currents.
* **Feedback System:** Uses a Hall effect encoder to process real-time speed feedback.
* **Performance:** The system was tuned to achieve a settling time of 3 seconds with minimal overshoot (less than 50 RPM).

### Technologies Used
* **Microcontroller:** TI MSP430
* **PCB Design:** KiCad
* **Hardware:** Custom H-Bridge with MOSFETs, Hall Effect Encoder

