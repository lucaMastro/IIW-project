#include <stdio.h>

/*	basically functions:
 *		-Syn
 *		-Syn_ack	
 * */

void stampa_mess(Message *mex){
	printf("\nMessage:\n");
	printf("seq_num: %u\n", mex -> seq_num);
	printf("ack_num: %u\n", mex -> ack_num);
	printf("flag:    %u\n", mex -> flag);
	//printf("rec_win: %u\n", mex -> rec_win);
	printf("length:  %u\n", mex -> length);
	
	if ((mex -> flag & CHAR_INDICATOR) == CHAR_INDICATOR)
		printf("data:    %s\n", mex -> list_data);
}
