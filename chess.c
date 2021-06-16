#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>

//May not need
#include <linux/fs.h>

/*  Only for old linux, use linux/uacces.h
#include <asm/uaccess.h>
*/
#include <linux/uaccess.h>
#include <linux/slab.h> /*Apparently need for kmalloc*/
#include <linux/miscdevice.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Phillip Chow");	
MODULE_DESCRIPTION("Chess Game");

#define MAX_BUFFER_READ 17 /*Max number of characters going to read in "01 WPf7-g8xBNyWB\n"*/


/*Return String Constants*/
#define UNKNOWN_CMD "UNKCMD\n"
#define BAD_FORMAT "INVFMT\n"
#define OK "OK\n"
#define WIN "WIN\n"
#define TIE "TIE\n"
#define OUT_OF_TURN "OOT\n"
#define ILL_MOVE "ILLMOVE\n"
#define NO_GAME "NOGAME\n"
#define CHECK "CHECK\n"
#define MATE "MATE\n"




#define MOVE_WHITE 1
#define MOVE_BLACK -1
#define MOVE_NONE 0

#define BAD_MOVE -1
#define VALID_MOVE 0
#define WIN_MOVE 1
#define CHECK_MOVE 2

#define WHITE_PAWN 1
#define WHITE_ROOK 2
#define WHITE_KNIGHT 3
#define WHITE_BISHOP 4
#define WHITE_QUEEN 5
#define WHITE_KING 6

#define BLACK_PAWN -1
#define BLACK_ROOK -2
#define BLACK_KNIGHT -3
#define BLACK_BISHOP -4
#define BLACK_QUEEN -5
#define BLACK_KING -6

#define EMPTY_SPACE 0
		
#define COL_A 0
#define COL_B 1
#define COL_C 2
#define COL_D 3
#define COL_E 4
#define COL_F 5
#define COL_G 6
#define COL_H 7

#define COMP_MOVE_MAX 2048



/*
Copying the tic-tac-toe code as basis for chess game
*/

/*Declare needed methods for kernal device
Then declare any other helper functions for chess */



static int Chess_open(struct inode* inode, struct file* filep); /*when device is opened?*/
static int Chess_release(struct inode* inode, struct file* filep); /*when device is turned off?*/

static ssize_t Chess_read(struct file* file, char __user * buffer, size_t length, loff_t* offset);/*How we read in input*/

static ssize_t Chess_write(struct file* file, const char __user * buffer, size_t length, loff_t* offset); /*writing back to user?*/


static struct file_operations Chess_fops =
{
	.owner = THIS_MODULE,
	.open = Chess_open,
	.read = Chess_read,
	.write = Chess_write,
	.release = Chess_release,
};

static struct miscdevice Chess_dev = 
{   .minor = MISC_DYNAMIC_MINOR,     
	.name = "chess",     
	.fops = &Chess_fops,
	.mode = 0777,
};



static int* theBoard; /*handles the games state, initialize in init*/
static int turn; /*who's turn*/
static int playerTurn; /*Track what human is playing as*/
static int gameStart; /*Signifies if the game is running*/

static char* returnString; /*for returning messages back to the user*/
static int numReturnString; /*For tracking size of return string*/

/*These will track the position of kings, used for check, checkmate*/
static int blackKingPos;
static int whiteKingPos;

/*These track when either player is in Check, or Checkmate.*/
static int checkState;
static int checkMateState;

static char theBoardString[129]; /*String for board print out (8x8 size board, with one character for \n)
								Order of print is white(a1, b1, c1 ...) to black (a7,b7,...g8,g8)
								Or bottom left to top right, two characters per block	*/


static void printBoard(void);
static void clearBoard(void); /*Will double as clearing and resetting*/

static int posIndex(int row, int col);


static int isValidMove(int givenTurn, int originPiece, int startIndex, int endIndex, int takePiece, int promotionPiece);

/*This function will check if the piece can reach it's destination, based on movesets and current obstacles. Will also temporarily apply move if possible, and check if Check against player is still active*/
static int isPossibleMove(int givenTurn, int originPiece, int startIndex, int endIndex, int takePiece);

/*checkCheck function will determine if the given player has a check advantage*/
static int checkCheck(int givenTurn);


static int tryMoveComp(int givenTurn, int originPiece, int startIndex); /*Overloaded for computer's move, updated for chess*/





/*The basic module parts of the program*/

/*Everything done when we load the device*/
/*"Chess" could be a constant instead of writing here*/
static int __init chess_init(void)
{
	
	int returnCode;
printk(KERN_ALERT "Starting to init...\n");

	/*recomended check if number not assigned*/
	printk(KERN_ALERT "Trying Misc_reg...\n");
	returnCode = misc_register(&Chess_dev);	
	if( returnCode != 0)
	{
		printk(KERN_ALERT "ERROR! Couldn't register major number for device\n Major returned: %d \n", returnCode);
		return returnCode;
	}
		printk(KERN_ALERT "Success\n");


	printk(KERN_ALERT "Loading Chess\n");
	
	/*Set up game board and variables*/
	turn = 0;
	playerTurn = 0;
	gameStart = 0;
	/* turnsLeft = 0; not really need for chess, maybe DELETE LATER */
	checkState = 0;
	checkMateState = 0;
	numReturnString = 0;
	
	returnString = (char*)kmalloc(129*sizeof(char),GFP_KERNEL); /*Large since board printout is 129 characters*/
	

	/*board is 8x8, so 64 integers*/
	theBoard = (int*)kmalloc(64*sizeof(int),GFP_KERNEL);
	printk(KERN_ALERT "Initializing game board...\n");

	clearBoard();

	

	theBoardString[128] = '\n';

	printk(KERN_ALERT "End of Init...\n");
	return 0;
}

/*Everything done when we remove the device (maybe free any data?)*/
/*again, "tictactoe" could be a constant we define*/
static void __exit chess_exit(void)
{
	int returnCode;
	printk(KERN_ALERT "un-Loading Chess\n");
	
	/*free anything made?*/
	printk(KERN_ALERT "freeing board...\n");
	kfree(theBoard);


	printk(KERN_ALERT "de registering....\n");
	misc_deregister(&Chess_dev);

printk(KERN_ALERT "done exit!\n");
}







	/********************************************************************************/


	/******************* Device Functions ***************************/


	/*******************************************************************************/








	/******************* Device Open   ***************************/


static int Chess_open(struct inode* inode, struct file* filep)
{
	
	/*
	//when opened/called?
	//This may be where we need to use mutex/semaphore to lock this
	//to count how many processes are using for read/write
	//Hold off for now, read somewhere on Piazza Hut that locking not necessary for this assignment
	*/
	

	return 0;
}




	/*******************Device Release   ***************************/


static int Chess_release(struct inode* indoe, struct file* filep)
{
	/*figure this one out, when unloaded
	Like said above, but maybe releasing mutex/semaphore*/
	
	return 0;
}





	/******************* Device Read   ***************************/


static ssize_t Chess_read(struct file* file, char __user * buffer, size_t length, loff_t * offset)
{
	/*
	//Allow user to read the latest return string
	//use copy_to_user function the returnString
	*/


	int numRead;

printk(KERN_ALERT "Starting to read...\n");
	
	
printk(KERN_ALERT "checking bad buffer and length...\n");
	if(buffer == NULL || length < 0)
	{
		return EFAULT;
	}
	
	if(length < numReturnString)
		numRead = length;
	else
		numRead = numReturnString;
	printk(KERN_ALERT "assigned lesser of read length...\n");
	

	printk(KERN_ALERT "trying copy to user...\n");
	if((copy_to_user(buffer, returnString, numRead)) != 0)
	{
		printk(KERN_ALERT "can't let user read, exiting...\n");
		
		/*Unlock any mutex if there is one*/
		return EFAULT;
	}
	
	/*		Nothing to do	*/
	printk(KERN_ALERT "copy to user /read success, closing...\n");
	
	/*Return number of read in bytes*/
	return numRead;
}






	/******************* Device Write   ***************************/


static ssize_t Chess_write(struct file* file, const char __user * buffer, size_t length, loff_t * offset)
{
	int numRead;
	unsigned char* copiedMessage;
	int iter;
	int tempInt;
	int x;
	int y;

	int i;


	/*For handling write command options*/
	int startPieceColor;
	int startPiece;
	int startRow;
	int startCol;
	
	int endRow;
	int endCol;

	int endPieceColor;
	int endPiece;
	int promotionPiece;

	/*Initialize some variables for simplicity*/
	startPieceColor = 0;
	startPiece = EMPTY_SPACE;
	startRow = -1;
	startCol = -1;
	
	endRow = -1;
	endCol = -1;

	endPieceColor = 0;
	endPiece = EMPTY_SPACE;
	promotionPiece = EMPTY_SPACE;

	printk(KERN_ALERT "Starting to write to device...\n");

	



	/*Check if permission to write*/


	printk(KERN_ALERT "check buffer null...\n");

	if(buffer == NULL)
	{
		printk(KERN_ALERT "..buffer null, return error\n");
		return EFAULT;
	}
	printk(KERN_ALERT "check if input length < 2...\n");
	/*Only valid inputs are at least 2 characters*/
	if(length < 2)
	{
		printk(KERN_ALERT "...input length too short, exiting!\n");
		return EFAULT; 
	}
	
	if(length < MAX_BUFFER_READ)
		numRead = length;
	else
		numRead = MAX_BUFFER_READ;
	printk(KERN_ALERT "assigned shorter write length...\n");

	printk(KERN_ALERT "K RE allocating copied message...\n");
	copiedMessage = (unsigned char*)kmalloc(numRead, GFP_KERNEL); /*Will initialize if not already*/
	printk(KERN_ALERT "...Realloc success!...\n");

	printk(KERN_ALERT "checking if valid can copy from user...\n");
	if((copy_from_user(copiedMessage, buffer, numRead)) != 0)
	{
		printk(KERN_ALERT "...Invalid copy from user...\n");
		/*Free copiedMessage, return error*/
		printk(KERN_ALERT "freeing copied message...\n");
		kfree(copiedMessage);
		printk(KERN_ALERT "...exiting with error!\n");
		/*Unlock any mutex if there is one*/
		return EFAULT;
	}
	





	/* Read in and interpret Commands */


	printk(KERN_ALERT "\n...Big Parser Part!\n");
	if(copiedMessage[0] != '0')
	{
		numReturnString = 7;
		returnString = UNKNOWN_CMD;
	}
	else
	{





	/******************* Command 00 (Start Game)   ***************************/


		if(copiedMessage[1] == '0')
		{
			printk(KERN_ALERT "given code 00...\n");
			if(copiedMessage[2] != ' ')
			{
				printk(KERN_ALERT "BUT NO SPACE AFTER, EXITING!...\n");
				kfree(copiedMessage);
				numReturnString = 7;
				returnString = BAD_FORMAT;
				return numRead;
			}
			if(copiedMessage[3] == 'W' || copiedMessage[3] == 'w')
			{
				printk(KERN_ALERT "Setting player White...\n");
				tempInt = MOVE_WHITE;
			}
			else if(copiedMessage[3] == 'B' || copiedMessage[3] == 'b')
			{
				printk(KERN_ALERT "Setting Player Black...\n");
				tempInt = MOVE_BLACK;
			}
			else
			{
				printk(KERN_ALERT "...Invalid player init character, exiting!!\n");
				kfree(copiedMessage);
				returnString = UNKNOWN_CMD;
				numReturnString = 7;
				return numRead;
			}

			/*Check if nothing else typed in*/
				printk(KERN_ALERT "Checking if remaining is space or new line...\n");
			iter = 4;
			while(iter < numRead)
			{
				if(copiedMessage[iter] != ' ' && copiedMessage[iter] != '\n')
				{
					printk(KERN_ALERT "...Character at %i is not space or newline, exiting!\n", iter);
					kfree(copiedMessage);
					returnString = UNKNOWN_CMD;
					numReturnString = 7;
					return numRead;
				}
				iter++;
			}
			printk(KERN_ALERT "Strating new game variables...\n");
			/*Start new Game*/
			clearBoard();
			gameStart = 1;
			turn = MOVE_WHITE;
			playerTurn = tempInt;
			/*turnsLeft = 9;*/
			/*Set position of Kings*/
			blackKingPos = posIndex(8, COL_E);
			whiteKingPos = posIndex(1, COL_E);

			printk(KERN_ALERT "Setting return string...\n");
			returnString = OK;
			printk(KERN_ALERT "return message is...");
			printk(KERN_ALERT "%s",returnString);
			numReturnString = 3;
		}







	/******************* Command 01 (Print Board)   ***************************/


		else if(copiedMessage[1] == '1')
		{
			printk(KERN_ALERT "Command is print board...\n");
			if(numRead >2) /*Check any remaining characters and ensure empty*/
			{
				iter = 2;
				while(iter < numRead)
				{
					if(copiedMessage[iter] != ' ' && copiedMessage[iter] != '\n')
					{
						printk(KERN_ALERT "...Character at %i is not space or newline, exiting!\n", iter);
						kfree(copiedMessage);
						returnString = BAD_FORMAT;
						numReturnString = 7;
						return numRead;
					}
					iter++;
				}
			}
			
			if(gameStart != 1)
			{
				printk(KERN_ALERT "...Game isn't running, exiting!!\n");
				kfree(copiedMessage);
				returnString = NO_GAME;
				numReturnString = 7;
				return numRead;
			}

			printBoard();


		}





	/******************* Command 02 (Player Makes Move)   ***************************/


		else if(copiedMessage[1] == '2')
		{
			printk(KERN_ALERT "Making move...\n");
			if(copiedMessage[2] != ' ')
			{
				printk(KERN_ALERT "...But NO SPACE AFTER COMMAND! Exiting\n");
				kfree(copiedMessage);
				returnString = UNKNOWN_CMD;
				numReturnString = 7;
				return numRead;
			}
			if(gameStart != 1)
			{
				printk(KERN_ALERT "...But GAME HASNT STARTED! Exiting\n");
				kfree(copiedMessage);
				returnString = NO_GAME;
				numReturnString = 7;
				return numRead;
			}
			if(playerTurn != turn)
			{
				printk(KERN_ALERT "...But Not TURN! Exiting\n");
				kfree(copiedMessage);
				returnString = OUT_OF_TURN;
				numReturnString = 4;
				return numRead;
			}





			/*  Read in move commands  */



			printk(KERN_ALERT "Checking start PieceColor from character 3...\n");
			if(copiedMessage[3] != 'B' && copiedMessage[3] !='W')
			{
				printk(KERN_ALERT "...But not a valid Color! Exiting\n");
				kfree(copiedMessage);
				returnString = BAD_FORMAT;
				numReturnString = 7;
				return numRead;
			}
			startPieceColor = copiedMessage[3];
			
			switch(copiedMessage[4])
			{
				case 'K':
					startPiece = (startPieceColor == 'B') ? BLACK_KING : WHITE_KING;
					break;
				case 'Q':
					startPiece = (startPieceColor == 'B') ? BLACK_QUEEN : WHITE_QUEEN;
					break;
				case 'R':
					startPiece = (startPieceColor == 'B') ? BLACK_ROOK : WHITE_ROOK;
					break;
				case 'N':
					startPiece = (startPieceColor == 'B') ?  BLACK_KNIGHT : WHITE_KNIGHT;
					break;
				case 'B':
					startPiece = (startPieceColor == 'B') ? BLACK_BISHOP :  WHITE_BISHOP;
					break;
				case 'P':
					startPiece = (startPieceColor == 'B') ? BLACK_PAWN : WHITE_PAWN;
					break;
				default:
					printk(KERN_ALERT "...But not a valid Piece! Exiting\n");
					kfree(copiedMessage);
					returnString = BAD_FORMAT;
					numReturnString = 7;
					return numRead;
					break;
			}

			
			

			switch(copiedMessage[5])
			{
				case 'A':
				case 'a':
					startCol = COL_A;
					break;
				case 'B':
				case 'b':
					startCol = COL_B;
					break;
				case 'C':
				case 'c':
					startCol = COL_C;
					break;
				case 'D':
				case 'd':
					startCol = COL_D;
					break;
				case 'E':
				case 'e':
					startCol = COL_E;
					break;
				case 'F':
				case 'f':
					startCol = COL_F;
					break;
				case 'G':
				case 'g':
					startCol = COL_G;
					break;
				case 'H':
				case 'h':
					startCol = COL_H;
					break;
				default:
					printk(KERN_ALERT "...Invalid Origin Col! Exiting\n");
					kfree(copiedMessage);
					returnString = BAD_FORMAT;
					numReturnString = 7;
					return numRead;
					break;
			}
			

			if(!(copiedMessage[6] - '0' > 0 && copiedMessage[6] - '0' < 9))
			{
				printk(KERN_ALERT "...Invalid Origin Row! Exiting\n");
				kfree(copiedMessage);
				returnString = BAD_FORMAT;
				numReturnString = 7;
				return numRead;
			}
			startRow = copiedMessage[6] - '0';


			if(copiedMessage[7] != '-' )
			{
				printk(KERN_ALERT "...Missing Dash! Exiting\n");
				kfree(copiedMessage);
				returnString = BAD_FORMAT;
				numReturnString = 7;
				return numRead;
			}


			
			

			switch(copiedMessage[8])
			{
				case 'A':
				case 'a':
					endCol = COL_A;
					break;
				case 'B':
				case 'b':
					endCol = COL_B;
					break;
				case 'C':
				case 'c':
					endCol = COL_C;
					break;
				case 'D':
				case 'd':
					endCol = COL_D;
					break;
				case 'E':
				case 'e':
					endCol = COL_E;
					break;
				case 'F':
				case 'f':
					endCol = COL_F;
					break;
				case 'G':
				case 'g':
					endCol = COL_G;
					break;
				case 'H':
				case 'h':
					endCol = COL_H;
					break;
				default:
					printk(KERN_ALERT "...Invalid End Col! Exiting\n");
					kfree(copiedMessage);
					returnString = BAD_FORMAT;
					numReturnString = 7;
					return numRead;
					break;
			}

			if(!(copiedMessage[9] - '0' > 0 && copiedMessage[9] - '0' < 9))
			{
				printk(KERN_ALERT "...Invalid end Row! Exiting\n");
				kfree(copiedMessage);
				returnString = BAD_FORMAT;
				numReturnString = 7;
				return numRead;
			}
			endRow = copiedMessage[9] - '0';


	/*    Big optional capture/promotion section, need to use numRead*/



			if(numRead == 11 || numRead == 12 || numRead == 13) /*Too short to do another command*/
			{

				iter = 10;
				printk(KERN_ALERT "Checking if rest are spaces or newline...\n");
				while(iter < numRead)
				{
					if(copiedMessage[iter] != ' ' && copiedMessage[iter] != '\n')
					{
						printk(KERN_ALERT "...None valid Blank character at %i ! Exiting\n", iter);
						kfree(copiedMessage);
						returnString = BAD_FORMAT;
						numReturnString = 7;
						return numRead;
					}
					iter++;
				}
			}




			if(numRead > 13 )
			{
				/*Larger enough for an action*/
				switch(copiedMessage[10])
				{
					case 'X':
					case 'x':
						if(numRead < 14)
						{
							printk(KERN_ALERT "...Wont have enough space to finish take command! Exiting\n");
							kfree(copiedMessage);
							returnString = BAD_FORMAT;
							numReturnString = 7;
							return numRead;
						}

						/*take action, will need to also check if also promotion if large enough*/
						printk(KERN_ALERT "Checking capture PieceColor from character 11...\n");
						if(!(copiedMessage[11] == 'B' || copiedMessage[11] =='W'))
						{
							printk(KERN_ALERT "...But not a valid Color! Exiting\n");
							kfree(copiedMessage);
							returnString = BAD_FORMAT;
							numReturnString = 7;
							return numRead;
						}
						endPieceColor = copiedMessage[11];
						
						switch(copiedMessage[12])
						{
							case 'K':
								endPiece = (endPieceColor == 'B') ?  BLACK_KING :  WHITE_KING;
								break;
							case 'Q':
								endPiece = (endPieceColor == 'B') ?  BLACK_QUEEN : WHITE_QUEEN;
								break;
							case 'R':
								endPiece = (endPieceColor == 'B') ? BLACK_ROOK :  WHITE_ROOK;
								break;
							case 'N':
								endPiece = (endPieceColor == 'B') ? BLACK_KNIGHT :  WHITE_KNIGHT;
								break;
							case 'B':
								endPiece = (endPieceColor == 'B') ? BLACK_BISHOP :  WHITE_BISHOP;
								break;
							case 'P':
								endPiece = (endPieceColor == 'B') ?  BLACK_PAWN : WHITE_PAWN;
								break;
							default:
								printk(KERN_ALERT "...But not a valid Piece! Exiting\n");
								kfree(copiedMessage);
								returnString = BAD_FORMAT;
								numReturnString = 7;
								return numRead;
								break;
						}
						
						
							
						switch(copiedMessage[13])
						{
							case '\n':
								break;

							case 'Y':
							case 'y':
								if(numRead < 17)
								{
									printk(KERN_ALERT "...Wont have enough space to finish take command! Exiting\n");
									kfree(copiedMessage);
									returnString = BAD_FORMAT;
									numReturnString = 7;
									return numRead;
								}

								printk(KERN_ALERT "Checking promotion PieceColor from character 14...\n");
								if(!(copiedMessage[14] == 'B' || copiedMessage[14] =='W'))
								{
									printk(KERN_ALERT "...But not a valid Color! Exiting\n");
									kfree(copiedMessage);
									returnString = BAD_FORMAT;
									numReturnString = 7;
									return numRead;
								}
								endPieceColor = copiedMessage[14];
								
								switch(copiedMessage[15])
								{
									case 'K':
										promotionPiece = (endPieceColor == 'B') ?  BLACK_KING : WHITE_KING;
										break;
									case 'Q':
										promotionPiece = (endPieceColor == 'B') ? BLACK_QUEEN : WHITE_QUEEN;
										break;
									case 'R':
										promotionPiece = (endPieceColor == 'B') ?  BLACK_ROOK : WHITE_ROOK;
										break;
									case 'N':
										promotionPiece = (endPieceColor == 'B') ? BLACK_KNIGHT : WHITE_KNIGHT;
										break;
									case 'B':
										promotionPiece = (endPieceColor == 'B') ?  BLACK_BISHOP : WHITE_BISHOP;
										break;
									case 'P':
										promotionPiece = (endPieceColor == 'B') ? BLACK_PAWN :  WHITE_PAWN;
										break;
									default:
										printk(KERN_ALERT "...But not a valid Piece! Exiting\n");
										kfree(copiedMessage);
										returnString = BAD_FORMAT;
										numReturnString = 7;
										return numRead;
										break;
								}
								
								/*At position 17, should only be \n*/
								if(copiedMessage[16] != '\n')
								{
									printk(KERN_ALERT "...So close! But last character isn't a new line! Exiting\n");
									kfree(copiedMessage);
									returnString = BAD_FORMAT;
									numReturnString = 7;
									return numRead;
								}
								break;

							case ' ': /*check if rest is space and ends in \n*/
								iter = 14;
								printk(KERN_ALERT "Checking if rest are spaces or newline...\n");
								while(iter < numRead)
								{
									if(copiedMessage[iter] != ' ' && copiedMessage[iter] != '\n')
									{
										printk(KERN_ALERT "...None valid Blank character at %i ! Exiting\n", iter);
										kfree(copiedMessage);
										returnString = BAD_FORMAT;
										numReturnString = 7;
										return numRead;
									}
									iter++;
								}
								break;
							
							default:
								printk(KERN_ALERT "...not valid input format! Exiting\n");
								kfree(copiedMessage);
								returnString = BAD_FORMAT;
								numReturnString = 7;
								return numRead;
								break;
						}
						break;

					case 'Y':
					case 'y':
						if(numRead <14)
						{
							printk(KERN_ALERT "...Not enough space to finish command! Exiting\n");
							kfree(copiedMessage);
							returnString = BAD_FORMAT;
							numReturnString = 7;
							return numRead;
						}
						
						printk(KERN_ALERT "Checking promotion PieceColor from character 14...\n");
						if(!(copiedMessage[11] == 'B' || copiedMessage[11] =='W'))
						{
							printk(KERN_ALERT "...But not a valid Color! Exiting\n");
							kfree(copiedMessage);
							returnString = BAD_FORMAT;
							numReturnString = 7;
							return numRead;
						}
						endPieceColor = copiedMessage[11];
						
						switch(copiedMessage[12])
						{
							case 'K':
								promotionPiece = (endPieceColor == 'B') ?  BLACK_KING : WHITE_KING;
								break;
							case 'Q':
								promotionPiece = (endPieceColor == 'B') ? BLACK_QUEEN : WHITE_QUEEN;
								break;
							case 'R':
								promotionPiece = (endPieceColor == 'B') ? BLACK_ROOK : WHITE_ROOK;
								break;
							case 'N':
								promotionPiece = (endPieceColor == 'B') ? BLACK_KNIGHT : WHITE_KNIGHT;
								break;
							case 'B':
								promotionPiece = (endPieceColor == 'B') ? BLACK_BISHOP : WHITE_BISHOP;
								break;
							case 'P':
								promotionPiece = (endPieceColor == 'B') ?  BLACK_PAWN : WHITE_PAWN;
								break;
							default:
								printk(KERN_ALERT "...But not a valid Piece! Exiting\n");
								kfree(copiedMessage);
								returnString = BAD_FORMAT;
								numReturnString = 7;
								return numRead;
								break;
						}
						
						/*loop and ensure no other entry*/
						iter = 13;
						printk(KERN_ALERT "Checking if rest are spaces or newline...\n");
						while(iter < numRead)
						{
							if(copiedMessage[iter] != ' ' && copiedMessage[iter] != '\n')
							{
								printk(KERN_ALERT "...None valid Blank character at %i ! Exiting\n", iter);
								kfree(copiedMessage);
								returnString = BAD_FORMAT;
								numReturnString = 7;
								return numRead;
							}
							iter++;
						}
						break;

					case ' ':
					case '\n':
						iter = 10;
						printk(KERN_ALERT "Checking if rest are spaces or newline...\n");
						while(iter < numRead)
						{
							if(copiedMessage[iter] != ' ' && copiedMessage[iter] != '\n')
							{
								printk(KERN_ALERT "...None valid Blank character at %i ! Exiting\n", iter);
								kfree(copiedMessage);
								returnString = BAD_FORMAT;
								numReturnString = 7;
								return numRead;
							}
							iter++;
						}
						break;
					
					default:
						printk(KERN_ALERT "...Invalid End Col! Exiting\n");
						kfree(copiedMessage);
						returnString = BAD_FORMAT;
						numReturnString = 7;
						return numRead;
						break;
				}
			}





	/* Command input is correct, now check if command can actually be made... */

			



			/*call isValidMove() to check game logic if the move is valid*/
			
			tempInt = isValidMove(playerTurn, startPiece, posIndex(startRow, startCol), posIndex(endRow, endCol), endPiece, promotionPiece);

			if(tempInt == VALID_MOVE)
			{
				/*Got the okay to make move, doing so now...*/
				theBoard[posIndex(startRow, startCol)] = EMPTY_SPACE;
				theBoard[posIndex(endRow, endCol)] = startPiece;
				/*Promote in needs be*/
				if(promotionPiece != EMPTY_SPACE)
				{
					theBoard[posIndex(endRow, endCol)] = promotionPiece;
				}
				/*Update King position if needs be*/
				if(startPiece == WHITE_KING || startPiece == BLACK_KING)
				{
					if(playerTurn == MOVE_WHITE)
					{
						whiteKingPos = posIndex(endRow, endCol);
					}
					else
					{
						blackKingPos = posIndex(endRow, endCol);
					}
					
				}
				/*Remove any checks against player*/
				if(!checkCheck(playerTurn * -1))
				{
					/*Redundant, but ensures that any check state is removed*/
					printk(KERN_ALERT "No longer a check against this player!!!...\n");
					checkState = 0;
				}
				/*See if move made a Check against computer*/
				if(checkCheck(playerTurn))
				{
					/*Move results in CHECK!*/
					checkState = playerTurn * -1;
					
					turn = (-1) * turn;
					printk(KERN_ALERT "Move Valid AND and CHECK!!!...\n");
					kfree(copiedMessage);
					returnString = CHECK;
					numReturnString = 6;
					return numRead;
				}
				else
				{
					printk(KERN_ALERT "...Ending input with OK!\n");
					printk(KERN_ALERT "Freeing Copied Message...!\n");
					kfree(copiedMessage);
					printk(KERN_ALERT "Changing Turns from %i...!\n", turn);
					turn = (-1) * turn;
					printk(KERN_ALERT "to %i...!\n", turn);
					
					returnString = OK;
					numReturnString = 3;
					printk(KERN_ALERT "Returning message okNewline...!\n");
					return numRead;
				}
				
			
			}
			else
			{
				/*Command was correct, but gave bad move*/
				printk(KERN_ALERT "...That move was Illegal, closing\n");
				kfree(copiedMessage);
				returnString = ILL_MOVE;
				numReturnString = 8;
				return numRead;
				
			}
		}







	/******************* Command 03 (Computer Move)   ***************************/


		/*This will be tricky, plan on brute force go through each avaialbe piece, and iterate through each place on the board and see if the piece can go there*/
		/*If the cell has an item in it of another piece, automatically capture. If it's a pawn piece, deafult to promoting to queen*/


		else if(copiedMessage[1] == '3')
		{
			printk(KERN_ALERT "making PC go...\n");
			printk(KERN_ALERT "Checking if no more inputs...\n");
			iter = 2;
			while(iter < numRead)
			{
				if(copiedMessage[iter] != ' ' && copiedMessage[iter] != '\n')
				{
					printk(KERN_ALERT "...Non Empty character at %i! Exiting\n", iter);
					kfree(copiedMessage);
					returnString = BAD_FORMAT;
					numReturnString = 7;
					return numRead;
				}
				iter++;
			}
			if(gameStart != 1)
			{
				printk(KERN_ALERT "...Game not started, Exiting\n");
				kfree(copiedMessage);
				returnString = NO_GAME;
				numReturnString = 7;
				return numRead;
			}
			if((playerTurn * (-1)) != turn)
			{
				printk(KERN_ALERT "...Not PC's Turn, Exiting!\n");
				kfree(copiedMessage);
				returnString = OUT_OF_TURN;
				numReturnString = 4;
				return numRead;
			}

	
			
			/*Keep it simple for assignment, just iterate through and find empty spot*/
			printk(KERN_ALERT "Pc is now trying brute force...\n");
			
			x = 0; /*use to iterate to find a moveable piece*/
			tempInt = BAD_MOVE; /*Place holder, if a valid move is found will be updated. Otherwise, signals Computer has no more moves?*/
			while(x < 63)
			{
				/*Found a piece the computer can move*/	
				if(theBoard[x] * playerTurn * -1 > 0)
				{
					tempInt = tryMoveComp((playerTurn * -1), theBoard[x], x);
					if(tempInt != BAD_MOVE)
					{
						break;
					}
				}
				x++; /*Do you know humch trouble I was having not having you here, you litle punk*/

			}
				


			if(tempInt == CHECK_MOVE)
			{
				printk(KERN_ALERT "...Brute Force gave PC a CHECK advantage!! Exiting!\n");
				turn = (-1) * turn;
				checkState = playerTurn;
				kfree(copiedMessage);
				returnString = CHECK_MOVE;
				numReturnString = 6;
				return numRead;
			}
			else if(tempInt == VALID_MOVE)
			{
				
				
				printk(KERN_ALERT "...Brute Force Move Success\n");
				kfree(copiedMessage);
				printk(KERN_ALERT "Changing Turns from %i...!\n", turn);
				turn = (-1) * turn;
				printk(KERN_ALERT "to %i...!\n", turn);
				returnString = OK;
				numReturnString = 3;
				return numRead;
				
			}
			else
			{
				/*The only way i've found to check if Computer lossed, TODO figure better way to check winning game state*/
				printk(KERN_ALERT "...Brute Force Move FAILURE\n");
				kfree(copiedMessage);
				turn = 0;
				gameStart = 0;
				checkMateState = playerTurn * -1;
				returnString = MATE;
				numReturnString = 5;
				return numRead;
			}
				
			

		}




		


	/******************* Command 04 (Surrender Game)   ***************************/





		else if(copiedMessage[1] == '4')
		{
			printk(KERN_ALERT "Surrendering, checking if able to...\n");
			/*ensure the rest of the command is empty*/
			iter = 2;
			printk(KERN_ALERT "Checking if rest are spaces or newline...\n");
			while(iter < numRead)
			{
				if(copiedMessage[iter] != ' ' && copiedMessage[iter] != '\n')
				{
					printk(KERN_ALERT "...None valid Blank character at %i ! Exiting\n", iter);
					kfree(copiedMessage);
					returnString = BAD_FORMAT;
					numReturnString = 7;
					return numRead;
				}
				iter++;
			}

			/*TODO In check mate, game is over and has won (Apprently different from no game state)*/
			/*Set this as first check, since  nogame state is more general*/
			if(checkMateState == playerTurn * -1)
			{
				printk(KERN_ALERT "...But Player has alreay gotten checkmate! Exiting\n");
				kfree(copiedMessage);
				/*returnString =(char*)krealloc(returnString, 7, GFP_KERNEL);*/
				returnString = MATE;
				numReturnString = 5;
				return numRead;
			}

			/*Check if game is running and player's turn*/
			if(gameStart != 1)
			{
				printk(KERN_ALERT "...But GAME HASNT STARTED! Exiting\n");
				kfree(copiedMessage);
				returnString = NO_GAME;
				numReturnString = 7;
				return numRead;
			}
			if(playerTurn != turn)
			{
				printk(KERN_ALERT "...But Not TURN! Exiting\n");
				kfree(copiedMessage);
				returnString = OUT_OF_TURN;
				numReturnString = 4;
				return numRead;
			}

			/*Otherwise, go ahead and surrender*/
			checkMateState = playerTurn;
			gameStart = 0;		
			printk(KERN_ALERT "...Ending input with OK!\n");
			printk(KERN_ALERT "Freeing Copied Message...!\n");
			kfree(copiedMessage);	
			returnString = OK;
			numReturnString = 3;
			printk(KERN_ALERT "Returning message okNewline...!\n");
			return numRead;	

			

		}
		else
		{
			printk(KERN_ALERT "...Don't know command, Exiting\n");
			returnString = UNKNOWN_CMD;
			numReturnString = 7;
		}
	}
	
	printk(KERN_ALERT "Freeing Copiedd Message...\n");
	kfree(copiedMessage);
	printk(KERN_ALERT "...Free Success, Returning with num read in characters\n");
	/*Return number of read in bytes*/
	return numRead;
	
}









	/**********************************************************************************/


	/******************* Helper Functions   ***************************/


	/*********************************************************************************/









static void printBoard()
{
	int i, k;

	printk(KERN_ALERT "\n Printing Board...\n");
	i = 0;
	k = 0;


	while(i < 64)
	{
		switch(theBoard[i])
		{
			case WHITE_PAWN:
				
				theBoardString[k] = 'W';
				theBoardString[k+1] = 'P';
				break;
			case WHITE_ROOK:
				
				theBoardString[k] = 'W';
				theBoardString[k+1] = 'R';
				break;
			case WHITE_KNIGHT:
				
				theBoardString[k] = 'W';
				theBoardString[k+1] = 'N';
				break;
			case WHITE_BISHOP:
				
				theBoardString[k] = 'W';
				theBoardString[k+1] = 'B';
				break;
			case WHITE_QUEEN:
				
				theBoardString[k] = 'W';
				theBoardString[k+1] = 'Q';
				break;
			case WHITE_KING:
				
				theBoardString[k] = 'W';
				theBoardString[k+1] = 'K';
				break;
			case BLACK_PAWN: 

				theBoardString[k] = 'B';
				theBoardString[k+1] = 'P';
				break;
			case BLACK_ROOK:

				theBoardString[k] = 'B';
				theBoardString[k+1] = 'R';
				break;
			case BLACK_KNIGHT: 

				theBoardString[k] = 'B';
				theBoardString[k+1] = 'N';
				break;
			case BLACK_BISHOP:

				theBoardString[k] = 'B';
				theBoardString[k+1] = 'B';
				break;
			case BLACK_QUEEN:

				theBoardString[k] = 'B';
				theBoardString[k+1] = 'Q';
				break;
			case BLACK_KING:

				theBoardString[k] = 'B';
				theBoardString[k+1] = 'K';
				break;
			case EMPTY_SPACE:

				theBoardString[k] = '*';
				theBoardString[k+1] = '*';
				break;
		}
	
	
	

		i++;
		k += 2;
	}

	returnString = theBoardString;
	numReturnString = 129;
	returnString[128] = '\n';
	printk(KERN_ALERT "...Done Printing Board\n");
} 




static void clearBoard()
{
	int row, col;
	printk(KERN_ALERT "Clearing Board...\n");
	/*Pawns*/

	row = 2;
	for(col = COL_A; col <= COL_H; col++)
	{
		theBoard[posIndex(row,col)] = WHITE_PAWN;
	}
	row = 7;
	for(col = COL_A; col <= COL_H; col++)
	{
		theBoard[posIndex(row,col)] = BLACK_PAWN;
	}

	/*Manually set up other pieces (Kings and queens always face eachother)*/

	theBoard[posIndex(1, COL_A)] = WHITE_ROOK;
	theBoard[posIndex(1, COL_B)] = WHITE_KNIGHT;
	theBoard[posIndex(1, COL_C)] = WHITE_BISHOP;
	theBoard[posIndex(1, COL_D)] = WHITE_QUEEN;
	theBoard[posIndex(1, COL_E)] = WHITE_KING;
	theBoard[posIndex(1, COL_F)] = WHITE_BISHOP;
	theBoard[posIndex(1, COL_G)] = WHITE_KNIGHT;
	theBoard[posIndex(1, COL_H)] = WHITE_ROOK;

	theBoard[posIndex(8, COL_A)] = BLACK_ROOK;
	theBoard[posIndex(8, COL_B)] = BLACK_KNIGHT;
	theBoard[posIndex(8, COL_C)] = BLACK_BISHOP;
	theBoard[posIndex(8, COL_D)] = BLACK_QUEEN;
	theBoard[posIndex(8, COL_E)] = BLACK_KING;
	theBoard[posIndex(8, COL_F)] = BLACK_BISHOP;
	theBoard[posIndex(8, COL_G)] = BLACK_KNIGHT;
	theBoard[posIndex(8, COL_H)] = BLACK_ROOK;

	/*No man's land*/
	for(row = 3; row < 7; row++)
	{
		for(col = COL_A; col < COL_H; col++)
		{
			theBoard[posIndex(row,col)] = EMPTY_SPACE;
		}
	}
	
	
}



/*This function will check if the move given is legal*/
static int isValidMove(int givenTurn, int originPiece, int startIndex, int endIndex, int takePiece, int promotionPiece)
{

	printk(KERN_ALERT "\n *** In isValidMove() method...\n");

	/*check if player is grabbing right piece color (or an empty space)*/
	if(givenTurn * originPiece <= 0)
	{
		printk(KERN_ALERT "...Bad move! Player is grabbing the wrong piece color!\n");
		return BAD_MOVE;
	}
	/*check if correct piece implied*/
	if(originPiece != theBoard[startIndex])
	{
		printk(KERN_ALERT "...Bad move! Piece doesn't match what's on the board!\n");
		return BAD_MOVE;
	}
	/*Check if attempting to move to non-empty spot*/
	if(theBoard[endIndex] != EMPTY_SPACE && takePiece == EMPTY_SPACE)
	{
		printk(KERN_ALERT "...Bad move! Trying to move in non-empty spot!\n");
		return BAD_MOVE;
	}
	/*Check if attempting to move at all*/
	if(startIndex == endIndex)
	{
		printk(KERN_ALERT "...Bad move! Not even making a move!\n");
		return BAD_MOVE;
	}
	/*check if given a take piece, it's of the opposite color*/
	/*Note: special case would be extra credit things like en passant and castle moves*/
	if(takePiece * givenTurn > 0)
	{
		printk(KERN_ALERT "...Bad move! Piece trying to take is of same color!\n");
		return BAD_MOVE;	
	}
	/*Check if the take piece is what is implied*/
	if(takePiece != theBoard[endIndex])
	{
		printk(KERN_ALERT "...Bad move! Piece trying to take doesn't match what's on the board!\n");
		return BAD_MOVE;
	}
	/*check if trying to promote*/
	if(promotionPiece != EMPTY_SPACE)
	{
		/*Check if promoting to valid color*/
		if(givenTurn * promotionPiece <= 0)
		{
			printk(KERN_ALERT "...Bad move! Player is promoting to the wrong piece color!\n");
			return BAD_MOVE;
		}
		/*Check if moving piece is a pawn*/
		if(originPiece != WHITE_PAWN || originPiece != BLACK_PAWN)
		{
			printk(KERN_ALERT "...Bad move! Only Pawns can promote!\n");
			return BAD_MOVE;
		}
		/*Check if promotion isn't King or pawn*/
		if(promotionPiece == BLACK_KING || promotionPiece == WHITE_KING || promotionPiece == BLACK_PAWN || promotionPiece == WHITE_PAWN)
		{
			printk(KERN_ALERT "...Bad move! Not a Valid piece to promote to!\n");
			return BAD_MOVE;
		}
		/*Check if at end of the board*/
		if((originPiece == WHITE_PAWN && endIndex < 56) || (originPiece == BLACK_PAWN && endIndex > 7))
		{
			printk(KERN_ALERT "... Bad move! Pawn isn't at the end yet!\n");
			return BAD_MOVE;
		}
	}
	/*Check if pawn has reached end, must declare promotion*/
	if(( (originPiece == WHITE_PAWN && endIndex > 55) || (originPiece == BLACK_PAWN && endIndex < 8) ) && promotionPiece == EMPTY_SPACE)
	{
		printk(KERN_ALERT "...Bad move! The pawn needs to make a promotion call!\n");
		return BAD_MOVE;
	}
	
	/*****  Moves are legal, now call seperate function to see if possible to commit **/
	/*Note isPossibleMove() will, after verifying the move is possible, also check if the player is still in check mode if set already*/
	if(isPossibleMove(givenTurn, originPiece, startIndex, endIndex, takePiece) != VALID_MOVE)
	{
		printk(KERN_ALERT "...Bad move! The move command is valid, but not possible!\n");
		return BAD_MOVE;
	}
	
	/*Command is valid, and move is logically sound, give the okay to go*/
	else
		return VALID_MOVE;

	
	
}




/*This function will check if the piece can reach it's destination, based on movesets and current obstacles. Will also temporarily apply move if possible, and check if Check against player is still active*/
static int isPossibleMove(int givenTurn, int originPiece, int startIndex, int endIndex, int takePiece)
{
	
	/*Piece identification, regardless of color*/
	int pieceGeneric;

	/*Use these to deconstruct index for easier processing*/
	int startRow;
	int startCol;
	int endRow;
	int endCol;

	/*Used to check path to destination*/
	int pathCheck;

	printk(KERN_ALERT "\n *** In isPossibleMove() method...\n");
	startRow = startIndex / 8;
	startCol = startIndex % 8;
	endRow = endIndex / 8;
	endCol = endIndex % 8;

	pieceGeneric = (originPiece * -1 < 0) ?  originPiece : (originPiece * -1);

	/*First, determine the piece being moved, and switch base on move set*/
	switch(pieceGeneric)
	{
		case WHITE_PAWN:
			

			/*Only check where color would matter, determine if moving in right direction*/
			if(givenTurn == MOVE_WHITE )
			{
			
				
				/*Check if only 4 motions: diagonals, forward, and double forward*/
				if(endIndex != startIndex + 7 && endIndex != startIndex + 8 && endIndex != startIndex + 9 && endIndex != startIndex + 16 )
				{
					printk(KERN_ALERT "...Pawn move not possible, not of 4 possible moves!\n");
					return BAD_MOVE;
				}

				/*Check if double move forward issues*/
				if((endIndex == startIndex + 16) && startRow != 1)
				{
					printk(KERN_ALERT "...Pawn move not possible, trying to do illegal double forward!\n");

					return BAD_MOVE;
				}
				if((endIndex == startIndex + 16) && takePiece != EMPTY_SPACE)
				{
					printk(KERN_ALERT "...Pawn move not possible, cannot capture on double move forward!\n");

					return BAD_MOVE;
				}
				
				/*Check move forward issues*/
				if((endIndex == startIndex + 8) && takePiece != EMPTY_SPACE)
				{
					printk(KERN_ALERT "...Pawn move not possible, cannot capture on move forward!\n");

					return BAD_MOVE;
				}

				/*Check diagonal issues*/
				if(((endIndex == startIndex + 7) && startCol == 0) || ((endIndex == startIndex + 9) && startCol == 7) )
				{
					printk(KERN_ALERT "...Pawn move not possible, diagonal movement is off the board!\n");

					return BAD_MOVE;
				}
				if( ((endIndex == startIndex + 7) || (endIndex == startIndex + 9))  && takePiece == EMPTY_SPACE)
				{
					printk(KERN_ALERT "...Pawn move not possible, diagonal movement ins't trying to capture!\n");
					return BAD_MOVE;
				}
				
				/*Nothing left to check, give the thumbs up*/
				printk(KERN_ALERT "...Pawn move is valid, all good!\n");
				return VALID_MOVE;
			}
			/*Black piece*/
			else
			{
			
				
				/*Check if only 4 motions: diagonals, forward, and double forward*/
				if(endIndex != startIndex - 7 && endIndex != startIndex - 8 && endIndex != startIndex - 9 && endIndex != startIndex - 16 )
				{
					printk(KERN_ALERT "...Pawn move not possible, not of 4 possible moves!\n");
					return BAD_MOVE;
				}

				/*Check if double move forward issues*/
				if((endIndex == startIndex - 16) && startRow != 6)
				{
					printk(KERN_ALERT "...Pawn move not possible, trying to do illegal double forward!\n");
					return BAD_MOVE;
				}
				if((endIndex == startIndex - 16) && takePiece != EMPTY_SPACE)
				{
					printk(KERN_ALERT "...Pawn move not possible, cannot capture on double move forward!\n");
					return BAD_MOVE;
				}
				
				/*Check move forward issues*/
				if((endIndex == startIndex - 8) && takePiece != EMPTY_SPACE)
				{
					printk(KERN_ALERT "...Pawn move not possible, cannot capture on move forward!\n");
					return BAD_MOVE;
				}

				/*Check diagonal issues*/
				if(((endIndex == startIndex - 7) && startCol == 7) || ((endIndex == startIndex - 9) && startCol == 0) )
				{
					printk(KERN_ALERT "...Pawn move not possible, diagonal movement is off the board!\n");
					return BAD_MOVE;
				}
				if( ((endIndex == startIndex - 7) || (endIndex == startIndex - 9))  && takePiece == EMPTY_SPACE)
				{
					printk(KERN_ALERT "...Pawn move not possible, diagonal movement ins't trying to capture!\n");
					return BAD_MOVE;
				}
				
				/*Nothing left to check, give the thumbs up*/
				printk(KERN_ALERT "...Pawn move is valid, all good!\n");
				return VALID_MOVE;
			}


			break;

		case WHITE_ROOK:
			
			/*Case where move is neither same column or row*/
			if(startRow != endRow && startCol != endCol)
			{
				printk(KERN_ALERT "...Rook move not possible, doesn't move vertically or horizontally!\n");
				return BAD_MOVE;
			}

			/*Assuming passed first condition, and the row isn't maintained, check vertical move*/
			if(startRow != endRow)
			{
				/*Check vertical path up to (but not including) end index. Doesn't matter if capture or not*/
				pathCheck = startIndex;
				pathCheck += (endRow > startRow) ?  8 :  -8;
				while(pathCheck != endIndex)
				{
					if(theBoard[pathCheck] != EMPTY_SPACE)
					{
						printk(KERN_ALERT "...Rook move not possible, a piece is blocking the path to destination!\n");
						return BAD_MOVE;	
					}
					pathCheck += (endRow > startRow) ?  8 : -8;
				}
			}
			/*Assuming passed first condition, and the col isn't maintained, check horizontal move*/
			if(startCol != endCol)
			{
				/*Check vertical path up to (but not including) end index. Doesn't matter if capture or not*/
				pathCheck = startIndex;
				pathCheck += (endCol > startCol) ? 1 : -1;
				while(pathCheck != endIndex)
				{
					if(theBoard[pathCheck] != EMPTY_SPACE)
					{
						printk(KERN_ALERT "...Rook move not possible, a piece is blocking the path to destination!\n");
						return BAD_MOVE;	
					}
					pathCheck += (endCol > startCol) ? 1 : -1;
				}
			}

			break;

		case WHITE_KNIGHT:
			/*This one will be hard, plan should be check if changes by 2cols, 1row or 2 row, 1 col. Jumps should be allowed, so no block checking needed*/
			if(startRow - endRow == 0 || startCol - endCol == 0)
			{
				printk(KERN_ALERT "...Knight move not possible, can't do linear movements!\n");
				printk(KERN_ALERT "start Row: %i Start Col: %i End Row: %i End Col: %i\n", startRow, startCol, endRow, endCol);
				return BAD_MOVE;
			}

			/*Reuse pathCheck for holding a number*/
			pathCheck = startRow-endRow;
			pathCheck = (pathCheck < 0) ?  pathCheck * -1 :  pathCheck;

			if(pathCheck > 2)
			{
				printk(KERN_ALERT "...Knight move not possible, move is too vertical!\n");
				return BAD_MOVE;
			}
			if(pathCheck == 2)
			{
				/*Check horizonal mvement is only by one*/
				pathCheck = startCol-endCol;
				pathCheck = (pathCheck < 0) ?  pathCheck * -1 :  pathCheck;
				if(pathCheck > 1)
				{
					printk(KERN_ALERT "...Knight move not possible, after valid vertical, move is too horizontal!\n");
					return BAD_MOVE;
				}
			}
			else if(pathCheck == 1)
			{
				/*Check horizonal mvement is only by two*/
				pathCheck = startCol-endCol;
				pathCheck = (pathCheck < 0) ?  pathCheck * -1 :  pathCheck;
				if(pathCheck != 2)
				{
					printk(KERN_ALERT "...Knight move not possible, after valid vertical, horizontal movement is invalid!\n");
					return BAD_MOVE;
				}
			}
			break;

		case WHITE_BISHOP:
			/*First, check if movement maintains 1:1 ratio horintal and vertical movement*/

			/*Reuse pathCheck for holding a number*/
			pathCheck = startRow-endRow;
			pathCheck = (pathCheck < 0) ?  pathCheck * -1 :  pathCheck;
			if(pathCheck != startCol - endCol && pathCheck != endCol - startCol )
			{
				printk(KERN_ALERT "...Bishop move not possible, horizontal and vertical ratio not preserved!\n");
				return BAD_MOVE;
			}

			/*Now Check if path is blocked to target*/
			pathCheck = startIndex;
			pathCheck +=(endRow > startRow) ?  8 : -8;
			pathCheck += (endCol > startCol) ? 1 : -1;
			while(pathCheck != endIndex)
			{
				if(theBoard[pathCheck] != EMPTY_SPACE)
				{
					printk(KERN_ALERT "...Rook move not possible, a piece is blocking the path to destination!\n");
					return BAD_MOVE;	
				}
				pathCheck +=(endRow > startRow) ?  8 : -8;
				pathCheck += (endCol > startCol) ? 1 : -1;
			}
			
			break;

		case WHITE_QUEEN:
		
			/*Will handle like bishop and rook. First check if move is like a rook, if not then check if like bishop.*/
			if(startRow == endRow || startCol == endCol)
			{
				/*Rook move set*/
				if(startRow != endRow)
				{
					/*Check vertical path up to (but not including) end index. Doesn't matter if capture or not*/
					pathCheck = startIndex;
					pathCheck += (endRow > startRow) ?  8 : -8;
					while(pathCheck != endIndex)
					{
						if(theBoard[pathCheck] != EMPTY_SPACE)
						{
							printk(KERN_ALERT "...Queen move not possible, a piece is blocking the path to destination!\n");
							return BAD_MOVE;	
						}
						pathCheck += (endRow > startRow) ?  8 : -8;
					}
				}
				/*Assuming passed first condition, and the col isn't maintained, check horizontal move*/
				if(startCol != endCol)
				{
					/*Check vertical path up to (but not including) end index. Doesn't matter if capture or not*/
					pathCheck = startIndex;
					pathCheck +=(endCol > startCol) ? 1 : -1;
					while(pathCheck != endIndex)
					{
						if(theBoard[pathCheck] != EMPTY_SPACE)
						{
							printk(KERN_ALERT "...Queen move not possible, a piece is blocking the path to destination!\n");
							return BAD_MOVE;	
						}
						pathCheck +=(endCol > startCol) ? 1 : -1;
					}
				}
			}
			/*otherwise, check if bishop move set*/
			else
			{
				/*Reuse pathCheck for holding a number*/
				pathCheck = startRow-endRow;
				pathCheck = (pathCheck < 0) ?  pathCheck * -1 : pathCheck;
				if(pathCheck != startCol - endCol && pathCheck != endCol - startCol )
				{
					printk(KERN_ALERT "...Queen move not possible, horizontal and vertical ratio not preserved!\n");
					return BAD_MOVE;
				}

				/*Now Check if path is blocked to target*/
				pathCheck = startIndex;
				pathCheck += (endRow > startRow) ?  8 : -8;
				pathCheck += (endCol > startCol) ? 1 : -1;
				while(pathCheck != endIndex)
				{
					if(theBoard[pathCheck] != EMPTY_SPACE)
					{
						printk(KERN_ALERT "...Queen move not possible, a piece is blocking the path to destination!\n");
						return BAD_MOVE;	
					}
					pathCheck += (endRow > startRow) ?  8 : -8;
					pathCheck += (endCol > startCol) ? 1 : -1;
				}
			}
			break;

		case WHITE_KING:
			/*Check if only 8 motions: diagonals, forward, and backwards*/
			if(endIndex != startIndex + 7 && endIndex != startIndex + 8 && endIndex != startIndex + 9 && endIndex != startIndex +1 && endIndex != startIndex - 7 && endIndex != startIndex - 8 && endIndex != startIndex - 9 && endIndex != startIndex - 1)
			{
				printk(KERN_ALERT "...King move not possible, not of 8 possible moves!\n");
				return BAD_MOVE;
			}
			/*Anyt move ment that is horizontal in nature must be checked*/
			if( ((endIndex == startIndex + 1) || (endIndex == startIndex - 1)) && startRow != endRow)
			{
				printk(KERN_ALERT "...King move not possible, tyring to move horizontal off the board!\n");
				return BAD_MOVE;
			}
			/*Check diagonal moves*/
			if(    ( ((endIndex == startIndex + 7) || (endIndex == startIndex - 9)) && startCol == 0 ) || ( ((endIndex == startIndex + 9) || (endIndex == startIndex - 7)) && startCol == 7)     )
			{
				printk(KERN_ALERT "...King move not possible, diagonal movement is off the board!\n");
				return BAD_MOVE;
			}
			break;
			
	}


	/*At this point, move should be good to go, last check is if move is made, will it remove a Check on player, or will it give a Check on opponent?*/

	/*Play out move, ignore promotion*/
	if(checkState == givenTurn)
	{
		theBoard[startIndex] = EMPTY_SPACE;
		theBoard[endIndex] = originPiece;
		if(pieceGeneric == WHITE_KING)
		{
			if(givenTurn == MOVE_WHITE)
			{
				 whiteKingPos = endIndex;
			}
			else
			{
				 blackKingPos = endIndex;
			}
			
			
		}
		if(checkCheck(givenTurn * -1))
		{
			theBoard[startIndex] = originPiece;
			theBoard[endIndex] = takePiece;
			/*Move doesn't remove check, so invalid*/
			printk(KERN_ALERT "...Move invalid, still have a check against player!\n");
			return BAD_MOVE;
		}
		/*No check, roll back changes*/
		if(pieceGeneric == WHITE_KING)
		{
			if(givenTurn == MOVE_WHITE)
			{
				 whiteKingPos = startIndex;
			}
			else
			{
				 blackKingPos = startIndex;
			}
			
			
		}
		theBoard[startIndex] = originPiece;
		theBoard[endIndex] = takePiece;
	}
	



	/*Nothing left to check, give the thumbs up*/
	printk(KERN_ALERT "...move is valid, all good!\n");
	return VALID_MOVE;
			
}



/*checkCheck function will determine if the given player has a check advantage*/
static int checkCheck(int givenTurn)
{
	/*Plan, loop through each board position. If it's the players piece, use isPossibleMove() to reach opposing king*/
	int iter;
	int moveResult;
	int targetKing;
	int targetKingPos;

	printk(KERN_ALERT "\n *** In checkCheck() method...\n");

	if(givenTurn == MOVE_WHITE)
	{
		targetKingPos = blackKingPos;
		targetKing = BLACK_KING;
	}
	else
	{
		targetKingPos = whiteKingPos;
		targetKing = WHITE_KING;
	}

	
	printk(KERN_ALERT "Checking if there is Check state...\n");
	iter = 0;
	while(iter != 64)
	{
		if(theBoard[iter] * givenTurn > 0)
		{
			/*Check if this piece can reach opposing King's position*/
			moveResult = isPossibleMove(givenTurn,  theBoard[iter], iter, targetKingPos, targetKing);
			if(moveResult == VALID_MOVE)
			{
				/*Move is a Check, close out and return*/
				printk(KERN_ALERT "Check state is true!\n");
				return 1;
			}
		}
		iter++;/*POS....*/
	}

	/*At this point, no move can reach king*/
	printk(KERN_ALERT "...Check state not found!\n");
	return 0;	
	
	
}







static int tryMoveComp(int givenTurn, int originPiece, int startIndex)
{

	int index;
	int takePiece;
	int moveResult;
	int promotionPiece;
	int currentIter; /*Will use this to break infinite loops*/
	
	printk(KERN_ALERT "Computer trying move...\n");

	index = 0;
	while(index < 63)
	{
		takePiece = theBoard[index];

		if(originPiece == WHITE_PAWN && index > 55)
		{
			promotionPiece = WHITE_QUEEN;
		}
		else if(originPiece == BLACK_PAWN && index < 8)
		{
			promotionPiece = BLACK_QUEEN;
		}
		else
		{
			promotionPiece = EMPTY_SPACE;
		}

		moveResult = isValidMove(givenTurn, originPiece, startIndex, index,  takePiece,  promotionPiece);
		
		if(moveResult == VALID_MOVE)
		{
			/*Move is successful, go ahead and commit, and return either valid or CHECK*/
			printk(KERN_ALERT "...Computer found a valid move!!! Commiting and returning!!\n");


			theBoard[startIndex] = EMPTY_SPACE;
			theBoard[index] = originPiece;
			/*Promote in needs be*/
			if(promotionPiece != EMPTY_SPACE)
			{
				theBoard[index] = promotionPiece;
			}
			/*Update King position if needs be*/
			if(originPiece == WHITE_KING || originPiece == BLACK_KING)
			{
				if(givenTurn == MOVE_WHITE)
				{
					whiteKingPos = index;
				}
				else
				{
					blackKingPos = index;
				}
				
			}
			if(checkState == givenTurn) //The computer has a check against it, see if that is still true
			{
				/*Remove any checks against computer*/
				if(!checkCheck(givenTurn * -1))
				{
					/*Redundant, but ensures that any check state is removed*/
					printk(KERN_ALERT "No longer a check against computer!!!...\n");
					checkState = 0;
				}
				else
				{
					theBoard[startIndex] = originPiece;
					theBoard[index] = takePiece;

					/*Update King position if needs be*/
					if(originPiece == WHITE_KING || originPiece == BLACK_KING)
					{
						if(givenTurn == MOVE_WHITE)
						{
							whiteKingPos = startIndex;
						}
						else
						{
							blackKingPos = startIndex;
						}
						
					}
					/*Move doesn't remove check, so invalid*/
					printk(KERN_ALERT "...Move invalid, still check against computer after validating some how!!\n");
					index++;
					continue;
				}
			}

			/*See if move made a Check against computer*/
			if(checkCheck(givenTurn))
			{
				/*Move results in CHECK!*/
				checkState = givenTurn * -1;

				printk(KERN_ALERT "Computer Move Valid AND and CHECK!!!...\n");
				return CHECK_MOVE;
			}
			else
			{
				
				return VALID_MOVE;
			}
			
		}
		index++;/*The headaches from not having this line has given me....*/
	}
	
	
	/*This piece couldn't find a valid move for this piece...*/
	printk(KERN_ALERT "...Computer couldn't find a valid move!!\n");
	return BAD_MOVE;
	
	
}



/*Helper function for getting a position in an array based of Rows and colos*/
/*!!note, due to board notations, rows startat 1, not 0. This function accomodates for this*/
static int posIndex(int row, int col)
{
	return ((row-1)*8) + col;
}





module_init(chess_init);
module_exit(chess_exit);

