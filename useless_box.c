#include <avr/io.h>
#include <avr/pgmspace.h>

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "I2C.h"
#include "ssd1306.h"
#include "data.h"

/*
============================================
							Usefull INFO
=============================================

	=== Platform coordinates ===
	-- Switches from the right --
	Switch 1 = 1660
	Switch 2 = 2100
	Switch 3 = 2600
	Switch 4 = 3160

	=== Arm coordinates ===
	-- Switches from the right --
	Pause = 2000
	Switch 1 = 2200
	Switch 2 = 2200
	Switch 3 = 2300
	Switch 4 = 2300

*/

/*
============================================
							Declarations
=============================================
*/

unsigned char _i2c_add;

//Display variables
#define ON 1
#define OFF 0
#define DONE 2
#define NOPE 3
#define TRUE 1
#define FALSE 0

int switch_1_on = 0;
int switch_2_on = 0;
int switch_3_on = 0;
int switch_4_on = 0;

int switch_1_done = 0;
int switch_2_done = 0;
int switch_3_done = 0;
int switch_4_done = 0;

int clear = 0;

// Servo variables
#define REST_POS 2000
int is_sleeping = FALSE;
int go_to_sleep = FALSE;
int ready_to_show_off = FALSE;

//Strategy variables
char strategy[5][16] = {{0}};
int strategy_type = 0;

int switch_1_ready = FALSE;
int switch_2_ready = FALSE;
int switch_3_ready = FALSE;
int switch_4_ready = FALSE;

/*
====================================
							Queue
====================================
*/
struct Queue *q;
struct QNode *waiting_switch;

// A linked list (LL) node to store a queue entry
struct QNode
{
    int button;
    struct QNode *next;
};

// The queue, front stores the front node of LL and rear stores the
// last node of LL
struct Queue
{
    struct QNode *front, *rear;
};

int queue_is_empty() {
	int is = switch_1_on | switch_2_on | switch_3_on | switch_4_on;

	return !is;
}

// A utility function to create a new linked list node.
struct QNode* newNode(int k)
{
    struct QNode *temp = (struct QNode*)malloc(sizeof(struct QNode));
    temp->button = k;
    temp->next = NULL;
    return temp;
}

// A utility function to create an empty queue
struct Queue *createQueue()
{
    struct Queue *q = (struct Queue*)malloc(sizeof(struct Queue));
    q->front = q->rear = NULL;
    return q;
}

// The function to add a button k to q
void enQueue(struct Queue *q, int k)
{
    // Create a new LL node
    struct QNode *temp = newNode(k);

    // If queue is empty, then new node is front and rear both
    if (q->rear == NULL)
    {
       q->front = q->rear = temp;
       return;
    }

    // Add the new node at the end of queue and change rear
    q->rear->next = temp;
    q->rear = temp;
}

// Function to remove a button from given queue q
struct QNode *deQueue(struct Queue *q)
{
    // If queue is empty, return NULL.
    if (q->front == NULL)
       return NULL;

    // Store previous front and move front one node ahead
    struct QNode *temp = q->front;
    q->front = q->front->next;

    // If front becomes NULL, then change rear also as NULL
    if (q->front == NULL)
       q->rear = NULL;
    return temp;
}

/*
============================================
						INIT USELESS BOX
=============================================
*/

void init_queue(){
	q = createQueue();
	waiting_switch = NULL;
}

void init_switches(){
	/* Set switch number 1 as input */
	DDRA &= ~(1 << PA0);
	/* Activate pull-up resistance */
	PORTA |= (1 << PA0);

	/* Set switch number 2 as input */
	DDRA &= ~(1 << PA1);
	/* Activate pull-up resistance */
	PORTA |= (1 << PA1);

	/* Set switch number 3 as input */
	DDRA &= ~(1 << PA2);
	/* Activate pull-up resistance */
	PORTA |= (1 << PA2);

	/* Set switch number 4 as input */
	DDRA &= ~(1 << PA3);
	/* Activate pull-up resistance */
	PORTA |= (1 << PA3);

}

void init_button(){
	/* Set Button as input */
	DDRA &= ~(1 << PA5);

	/* Activate pull-up resistance */
	PORTA |= (1 << PA5);

}

void init_servos(){
	/* Set - prescale at 8
				 - FPWM with TOP at ICR1*/
	TCCR1A |= (1 << WGM11);
	TCCR1B |= (1 << WGM12) | (1 << WGM13) | (1 << CS11);

	/* Set ICR1 register: PWM period */
	ICR1 = 40000;

	/*==== Set platform servo as output ====*/
	DDRD |= (1 << PD5);
	/*==== Set arm servo as output ====*/
	DDRD |= (1 << PD4);
}

void init_debug(){
	/* Setez pinul PD7 ca iesire. */
	DDRD |= (1 << PD7);

	/* Sting LED-ul. */
	PORTD &= ~(1 << PD7);
}

void init_strategy(){
	strcpy(strategy[0], "CoffeeBreak");
	strcpy(strategy[1], "LazyBoy");
	strcpy(strategy[2], "WorkingBoy");
	strcpy(strategy[3], "Mr.ShowOff");
	strcpy(strategy[4], "Sleep");
}

void init_display(){
	_i2c_add = 0X78;
	I2C_Init();
	_delay_ms(10);
	/* Set display SDA on PC1 and SCK on PC0 */
	InitializeDisplay();
	reset_display();

	clear_display();
	/* Switch 4 */
	sendCharXY('-',0,2);

	sendCharXY('|',1,1);
	sendCharXY('|',1,3);

	sendCharXY('-',2,2);

	/* Switch 3 */
	sendCharXY('-',0,6);

	sendCharXY('|',1,5);
	sendCharXY('|',1,7);

	sendCharXY('-',2,6);

	/* Switch 2 */
	sendCharXY('-',0,10);

	sendCharXY('|',1,9);
	sendCharXY('|',1,11);

	sendCharXY('-',2,10);

	/* Switch 1 */
	sendCharXY('-',0,14);

	sendCharXY('|',1,13);
	sendCharXY('|',1,15);

	sendCharXY('-',2,14);

	sendStrXY("Current Strategy", 4, 0);
	sendStrXY(strategy[0],6,4);

	/* Button */
	sendCharXY('-',5,1);

	sendCharXY('|',6,0);
	sendCharXY('|',6,2);

	sendCharXY('-',7,1);

}

/*
============================================
							Debug button
=============================================
*/

/* Aprinde led-ul PD7 la apasarea butonului PA0. */
void buton(void) {
	/* Setez pinul PA0 ca intrare. */
	DDRA &= ~(1 << PA0);
	//DDRB &= ~(1 << PB2);

	/* Activez rezistenta de pull-up pentru pinul PA0. */
	PORTA |= (1 << PA0);
	//PORTB |= (1 << PB2);

	/* Setez pinul PD7 ca iesire. */
	DDRD |= (1 << PD7);

	/* Sting LED-ul. */
	//PORTD |= (1 << PD7);
	PORTD &= ~(1 << PD7);

	while (1) {
		/* Daca butonul este apasat. */
		if ((PINA & (1 << PA0)) == 0)
			PORTD |= (1 << PD7);
			//PORTB |= (1 << PB2);
			/* Aprind LED-ul. */
		else
			/* Sting LED-ul. */
			PORTD &= ~(1 << PD7);
	}
}

/*
============================================
						Working Strategies
=============================================
*/

void change_strategy(){
	strategy_type++;
	strategy_type %= 5;
}

/*
============================================
							Update Display
=============================================
*/

void draw_background(){
	clear_display();
	/* Switch 4 */
	sendCharXY('-',0,2);

	sendCharXY('|',1,1);
	sendCharXY('|',1,3);

	sendCharXY('-',2,2);

	/* Switch 3 */
	sendCharXY('-',0,6);

	sendCharXY('|',1,5);
	sendCharXY('|',1,7);

	sendCharXY('-',2,6);

	/* Switch 2 */
	sendCharXY('-',0,10);

	sendCharXY('|',1,9);
	sendCharXY('|',1,11);

	sendCharXY('-',2,10);

	/* Switch 1 */
	sendCharXY('-',0,14);

	sendCharXY('|',1,13);
	sendCharXY('|',1,15);

	sendCharXY('-',2,14);
}

/*
============================================
							Strategy Actions
=============================================
*/

void go_sleep(){
	TCCR1A |= (1 << COM1A1) |  (1<<COM1A0);
	OCR1A = ICR1 - 2400;
	_delay_ms(500);
	TCCR1A |= (1 << COM1B1) |  (1<<COM1B0);
	OCR1B = ICR1 - 2000;
	_delay_ms(500);
	OCR1B = ICR1 - 2200;
	_delay_ms(500);
	OCR1B = ICR1 - 2360;
	_delay_ms(100);
	OCR1B = ICR1 - 2400;
	_delay_ms(100);
	OCR1B = ICR1 - 2460;
	_delay_ms(500);

}

void get_ready(){
	TCCR1A |= (1 << COM1B1) |  (1<<COM1B0);
	OCR1B = ICR1 - 2000;
	_delay_ms(100);
	TCCR1A |= (1 << COM1A1) |  (1<<COM1A0);
	OCR1A = ICR1 - 2700;
	_delay_ms(500);
}


void show_off(){
	/* Pause */
	TCCR1A |= (1 << COM1A1) |  (1<<COM1A0);
	OCR1A = ICR1 - 2700;
	_delay_ms(100);
	TCCR1A |= (1 << COM1B1) |  (1<<COM1B0);
	OCR1B = ICR1 - 2000;
	_delay_ms(500);

	/* Provoke */
	TCCR1A |= (1 << COM1A1) |  (1<<COM1A0);
	OCR1A = ICR1 - 1600;
	_delay_ms(300);
	OCR1A = ICR1 - 3200;
	_delay_ms(1000);
	OCR1A = ICR1 - 1600;
	_delay_ms(300);
	OCR1A = ICR1 - 3200;
	_delay_ms(1000);

	/* Button 1 */
	TCCR1A |= (1 << COM1A1) |  (1<<COM1A0);
	OCR1A = ICR1 - 1660;
	_delay_ms(170);
	TCCR1A |= (1 << COM1B1) |  (1<<COM1B0);
	OCR1B = ICR1 - 2200;
	_delay_ms(170);
	OCR1B = ICR1 - 2100;
	_delay_ms(170);

	/* Button 2 */
	TCCR1A |= (1 << COM1A1) |  (1<<COM1A0);
	OCR1A = ICR1 - 2100;
	_delay_ms(170);
	TCCR1A |= (1 << COM1B1) |  (1<<COM1B0);
	OCR1B = ICR1 - 2200;
	_delay_ms(170);
	OCR1B = ICR1 - 2100;
	_delay_ms(170);

	/* Button 3 */
	TCCR1A |= (1 << COM1A1) |  (1<<COM1A0);
	OCR1A = ICR1 - 2600;
	_delay_ms(170);
	TCCR1A |= (1 << COM1B1) |  (1<<COM1B0);
	OCR1B = ICR1 - 2300;
	_delay_ms(170);
	OCR1B = ICR1 - 2100;
	_delay_ms(170);

	/* Button 4 */
	TCCR1A |= (1 << COM1A1) |  (1<<COM1A0);
	OCR1A = ICR1 - 3160;
	_delay_ms(170);
	TCCR1A |= (1 << COM1B1) |  (1<<COM1B0);
	OCR1B = ICR1 - 2300;
	_delay_ms(170);
	OCR1B = ICR1 - 2100;
	_delay_ms(170);

	/* Pause */
	TCCR1A |= (1 << COM1A1) |  (1<<COM1A0);
	OCR1A = ICR1 - 2700;
	_delay_ms(100);
	TCCR1A |= (1 << COM1B1) |  (1<<COM1B0);
	OCR1B = ICR1 - 2000;
	_delay_ms(500);

}

int is_all_set() {
	int is = switch_1_ready & switch_2_ready & switch_3_ready & switch_4_ready;
	return is;
}

/*
=============================================
				Buttons Queue & Servo action
=============================================
*/

void update_buttons(){

	if(strategy_type != 1 && strategy_type != 2) {
		if(switch_1_on == ON) {
			sendCharXY('x',1,2);
		}
		if(switch_2_on == ON) {
			sendCharXY('x',1,6);
		}
		if(switch_3_on == ON) {
			sendCharXY('x',1,10);
		}
		if(switch_4_on == ON) {
			sendCharXY('x',1,14);
		}
	}


	/* Switch 1 is activated */
	if ((PINA & (1 << PA0)) == 0) {
		if(strategy_type == 1 || strategy_type == 2) {
			if(switch_1_on == OFF) {
				sendCharXY('*',1,2);
			}
		}
		if(switch_1_on == OFF) {
			if(ready_to_show_off == TRUE) {
				switch_1_ready = TRUE;
			} else {
				if(strategy_type == 1 || strategy_type == 2) {
					/* Activate button */
					switch_1_on = ON;
					enQueue(q, 1);
				}
			}
		}
	} else {
		if(strategy_type == 1 || strategy_type == 2) {
			sendCharXY(' ',1,2);
		}
		switch_1_ready = FALSE;
		if(switch_1_on == ON) {
			if(switch_1_done == DONE){
				switch_1_on = OFF;
				switch_1_done = NOPE;
			}
		}
	}

	/* Switch 2 is activated */
	if ((PINA & (1 << PA1)) == 0) {
		if(strategy_type == 1 || strategy_type == 2) {
			if(switch_2_on == OFF) {
				sendCharXY('*',1,6);
			}
		}
		if(switch_2_on == OFF) {
			if(ready_to_show_off == TRUE) {
				switch_2_ready = TRUE;
			} else {
				if(strategy_type == 1 || strategy_type == 2) {
					/* Activate button */
					switch_2_on = ON;
					enQueue(q, 2);
				}
			}
		}
	} else {
		if(strategy_type == 1 || strategy_type == 2) {
			sendCharXY(' ',1,6);
		}
		switch_2_ready = FALSE;
		if(switch_2_on == ON) {
			if(switch_2_done == DONE){
				switch_2_on = OFF;
				switch_2_done = NOPE;
			}
		}
	}

	/* Switch 3 is activated */
	if ((PINA & (1 << PA2)) == 0) {
		if(strategy_type == 1 || strategy_type == 2) {
			if(switch_3_on == OFF) {
				sendCharXY('*',1,10);
			}
		}
			if(switch_3_on == OFF) {
				if(ready_to_show_off == TRUE) {
					switch_3_ready = TRUE;
				} else {
					if(strategy_type == 1 || strategy_type == 2) {
						/* Activate button */
						switch_3_on = ON;
						enQueue(q, 3);
					}
				}
			}
	} else {
		if(strategy_type == 1 || strategy_type == 2) {
			sendCharXY(' ',1,10);
		}
		switch_3_ready = FALSE;
		if(switch_3_on == ON) {
			if(switch_3_done == DONE){
				switch_3_on = OFF;
				switch_3_done = NOPE;
			}
		}
	}

	/* Switch 4 is activated */
	if ((PINA & (1 << PA3)) == 0) {
		if(strategy_type == 1 || strategy_type == 2) {
			if(switch_4_on == OFF) {
				sendCharXY('*',1,14);
			}
		}
		if(switch_4_on == OFF) {
			if(ready_to_show_off == TRUE) {
				switch_4_ready = TRUE;
			} else {
				if(strategy_type == 1 || strategy_type == 2) {
					/* Activate button */
					switch_4_on = ON;
					enQueue(q, 4);
				}
			}
		}
	} else {
		if(strategy_type == 1 || strategy_type == 2) {
			sendCharXY(' ',1,14);
		}
		switch_4_ready = FALSE;
		if(switch_4_on == ON) {
			if(switch_4_done == DONE){
				switch_4_on = OFF;
				switch_4_done = NOPE;
			}
		}
	}

	/* Button is pressed */
	if ((PINA & (1 << PA5)) == 0) {
		sendCharXY('*',6,1);

		if(is_sleeping == TRUE) {
			is_sleeping = FALSE;
			get_ready();
		}

		ready_to_show_off = FALSE;
		switch_1_ready = FALSE;
		switch_2_ready = FALSE;
		switch_3_ready = FALSE;
		switch_4_ready = FALSE;

		change_strategy();
		sendStrXY("            ",6,3);
		if(strategy_type == 0) {
			sendStrXY(strategy[strategy_type],6,4);
		} else {
			if(strategy_type == 1) {
				sendStrXY(strategy[strategy_type],6,6);
		} else {
			if(strategy_type == 2){
				sendStrXY(strategy[strategy_type],6,5);
		} else {
			if(strategy_type == 3) {
				sendStrXY(strategy[strategy_type],6,5);
				if(queue_is_empty() == 1) {
					ready_to_show_off = TRUE;
				}
			} else {
				if(strategy_type == 4) {
					sendStrXY(strategy[strategy_type],6,7);
						if(queue_is_empty() == 1) {
							go_to_sleep = TRUE;
							sendCharXY(' ',1,2);
							sendCharXY(' ',1,6);
							sendCharXY(' ',1,10);
							sendCharXY(' ',1,14);
						}
					}
				}
			}
		}
	}
	_delay_ms(200);
	sendCharXY(' ',6,1);
	}
}

void activate_switch(int platform_coord, int arm_coord){
	/* Move platform */
	TCCR1A |= (1 << COM1A1) |  (1<<COM1A0);
	OCR1A = ICR1 - platform_coord;
	_delay_ms(200);

	/* Move arm */
	TCCR1A |= (1 << COM1B1) |  (1<<COM1B0);
	OCR1B = ICR1 - arm_coord;
	_delay_ms(200);
	OCR1B = ICR1 - REST_POS;
	_delay_ms(200);
}

void be_useless(struct QNode *node){
	switch (node->button) {
		case 1: activate_switch(1660, 2200); switch_1_done = DONE; break;
		case 2: activate_switch(2100, 2200); switch_2_done = DONE; break;
		case 3: activate_switch(2600, 2300); switch_3_done = DONE; break;
		case 4: activate_switch(3160, 2300); switch_4_done = DONE; break;
	}
}

void check_for_job(){
	struct QNode *node = deQueue(q);
	/* If there is an activate switch*/
	if(node != NULL){
		if(waiting_switch != NULL) {
			be_useless(waiting_switch);
			be_useless(node);
			free(waiting_switch);
			waiting_switch = NULL;
		} else {
			if(strategy_type == 1) {
				waiting_switch = node;
				return;
			} else {
				be_useless(node);
			}
		}
	} else {
		if(waiting_switch != NULL && strategy_type == 2) {
			be_useless(waiting_switch);
			waiting_switch = NULL;
			return;
		}
	}
	free(node);
}

/*
=============================================
								Main Program
=============================================
*/

int main(void) {

	init_servos();
	init_queue();
	init_strategy();
	init_button();
	init_display();
	init_switches();

	get_ready();

	while(1){
		update_buttons();

		if(strategy_type == 1 || strategy_type == 2) {
			check_for_job();
		}

		if(go_to_sleep == TRUE) {
			go_sleep();
			is_sleeping = TRUE;
			go_to_sleep = FALSE;
		}

		if(ready_to_show_off == TRUE && is_all_set() == TRUE){
			show_off();
		}

	}
	return 0;
}
