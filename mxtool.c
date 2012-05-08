/********
Kevin Tran
0599585
CIS2750
Assignment 2 
mxtool.c
upper layer for mxutil.c - utilizes library functions to write records to a file
********/
#define _POSIX_SOURCE 1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <termios.h>
#include <regex.h>
#include <Python.h>
#include "mxutil.h"
#include "mxtool.h"
#include <libxml/xmlschemastypes.h>

int comp (const void *a, const void *b);

int main (int argc, char *argv[], char *envp[]) {
	XmElem *top, *top2;
	xmlSchemaPtr sp = NULL;	
	const char *xsdfile;
	int status;

	FILE *devtty = fopen( "/dev/tty", "w" );
	if(!devtty) {
		fprintf(stderr, "Unable to open /dev/tty\n");
		fclose(devtty);
		exit(1);
	}

	xsdfile = getenv("MXTOOL_XSD"); //note to self: export MXTOOL_XSD=path to create envvar
	sp = mxInit(xsdfile);
	if (sp == NULL) {
		fprintf(stderr, "Could not create/parse XSD schema\n");
		fclose(devtty);
		mxTerm(sp);
		exit(1);
	}
	status = mxReadFile(stdin, sp, &top); //read in main xml file
	if (status == 1) {
		fprintf(stderr, "Failed to parse XML file\n");
		fclose(devtty);
		mxTerm(sp);
		fclose(stdin);
		exit(1);
	} else if (status == 2) {
		fprintf(stderr, "XML did not match schema\n");
		fclose(devtty);
		mxTerm(sp);
		fclose(stdin);
		exit(1);
	}
	fclose(stdin);

	if (argc > 1) {
		if (strcmp(argv[1], "-review") == 0) {
			status = review(top, stdout);
		} else if (strcmp(argv[1], "-lib") == 0) {
			status = libFormat(top, stdout);	
		} else if (strcmp(argv[1], "-bib") == 0) {
			status = bibFormat(top, stdout);	
		} else if (strcmp(argv[1], "-cat") == 0) {
			FILE *extra = fopen(argv[2], "r");
			status = mxReadFile(extra, sp, &top2);
			if (status == 1) {
				fprintf(stderr, "Failed to parse XML file\n");
				fclose(extra);
				exit(1);
			} else if (status == 2) {
				fprintf(stderr, "XML did not match schema\n");
				fclose(extra);
				exit(1);
			}
			status = concat(top, top2, stdout);

			mxCleanElem(top2);
			fclose(extra);	
		} else if (strcmp(argv[1], "-keep") == 0) {
			status = selects(top, KEEP, argv[2], stdout);
		} else if (strcmp(argv[1], "-discard") == 0) {
			status = selects(top, DISCARD, argv[2], stdout);
		} else {
			fprintf(devtty, "The correct syntax is:\n");
			fprintf(devtty, " mxtool -review < riscbib.xml > newbib.xml\n");
			fprintf(devtty, " mxtool -cat tombib.xml < riscbib.xml > big.xml\n");
			fprintf(devtty, " cat riscbib.xml | mxtool -keep t=RISC > short.xml\n");
			fprintf(devtty, " cat riscbib.xml | mxtool -discard p=19.. > new.xml\n");
			fprintf(devtty, " mxtool -lib < riscbib.xml\n");
			fprintf(devtty, " mxtool -bib < riscbib.xml\n");
		}
	}

	fclose(devtty);
	fclose(stdout);
	mxCleanElem(top);
	mxTerm(sp);
	return 0;
}

int review( const XmElem *top, FILE *outfile ) {
	XmElem *new;
	BibData bdata;
	char selected;
	int j = 0;
	struct termios initial_settings, new_settings;

	FILE *infile = fopen("/dev/tty", "r");
	FILE *output = fopen("/dev/tty", "w");
	if(!infile || !output) {
		fprintf(stderr, "Unable to open /dev/tty\n");
		fclose(infile);
		fclose(output);
		return EXIT_FAILURE;
	}

	tcgetattr(fileno(infile), &initial_settings);	//termios-remove echo + waiting for enter
	new_settings = initial_settings;
	new_settings.c_lflag &= ~ICANON;				//canonical option
	new_settings.c_lflag &= ~ECHO;					//echo option
	new_settings.c_cc[VMIN] = 1;
	new_settings.c_cc[VTIME] = 0;
	if(tcsetattr(fileno(infile), TCSANOW, &new_settings) != 0) {	//set dev/tty with settings
		fprintf(stderr,"could not set attributes\n");
		fclose(infile);
		fclose(output);
		tcsetattr(fileno(infile), TCSANOW, &initial_settings);
		return EXIT_FAILURE;
	}
	
	new = calloc(1, sizeof(struct XmElem));	//create a XmElem struct for shallow copying

	new->tag = top->tag;
	new->text = top->text;
	new->isBlank = top->isBlank;
	new->nattribs = top->nattribs;
	new->attrib = top->attrib;
	new->nsubs = 0;

	new->subelem = calloc(top->nsubs, sizeof(struct XmElem));

	for (int i=0; i < top->nsubs; i++) {		//loops through each record and stores bibdata
		marc2bib( (*top->subelem)[i], bdata);
		char *author, *title, *pubinfo;
		author = bdata[AUTHOR];
		title = bdata[PUBINFO];
		pubinfo = bdata[PUBINFO];
		if (author[strlen(author) - 1] == '.')
			fprintf(output, "%d. %s ", i + 1, bdata[AUTHOR]);
		else
			fprintf(output, "%d. %s. ", i + 1, bdata[AUTHOR]);
		if (title[strlen(title) - 1] == '.')
			fprintf(output, "%s ", bdata[TITLE]);
		else
			fprintf(output, "%s. ", bdata[TITLE]);
		if (pubinfo[strlen(pubinfo) - 1] == '.')
			fprintf(output, "%s\n\n", bdata[PUBINFO]);
		else
			fprintf(output, "%s.\n\n", bdata[PUBINFO]);
		
		selected = fgetc(infile);

		if (selected == ' ') {			//if char entered is space: skip record
		} else if (selected == '\n') {	//if enter is entered: create a shallow copy
			(*new->subelem)[j] = (*top->subelem)[i];
			j += 1;
		} else if (selected == 'k') {	//if k is entered: create copy of rest of collection
			for (int k = i; k < top->nsubs; k++) {
				(*new->subelem)[j] = (*top->subelem)[k];
				j += 1;
			}
			free(bdata[TITLE]);			//frees bibdata before it breaks out of loop
			free(bdata[AUTHOR]);
			free(bdata[PUBINFO]);
			free(bdata[CALLNUM]);
			break;
		} else if (selected == 'd') {
			(*new->subelem)[j] = NULL;	//if d is entered: stop copying all records
			free(bdata[TITLE]);
			free(bdata[AUTHOR]);
			free(bdata[PUBINFO]);
			free(bdata[CALLNUM]);
			break;
		} else {
			fprintf(output, "Enter: record is copied to stdout.\n");
			fprintf(output, "Spacebar: record is skipped.\n");
			fprintf(output, "'k' (keep): remaining records are copied and program exits.\n");
			fprintf(output, "'d' (discard): remaining records are skipped and program exits.\n");
			i -= 1;
		}
		free(bdata[TITLE]);
		free(bdata[AUTHOR]);
		free(bdata[PUBINFO]);
		free(bdata[CALLNUM]);
	}
	new->nsubs = j;
	int status = mxWriteFile(new, outfile);
	if (status == -1) {
		fprintf(stderr, "Error trying to write records to file\n");	
		fclose(infile);
		fclose(output);
		tcsetattr(fileno(infile), TCSANOW, &initial_settings);
		free(new->subelem);
		free(new);
		return EXIT_FAILURE;
	}

	tcsetattr(fileno(infile), TCSANOW, &initial_settings);
	free(new->subelem);
	free(new);

	fclose(infile);
	fclose(output);
	return EXIT_SUCCESS;
}

int concat( const XmElem *top1, const XmElem *top2, FILE *outfile ){
	XmElem *top = NULL;

	top = calloc(1, sizeof(XmElem));	//top is a pointer that will point to both top1 + top2
	
	top->tag = top1->tag;				//manually copying over collection record 
	top->text = top1->text;
	top->isBlank = top1->isBlank;
	top->nattribs = top1->nattribs;
	top->attrib = top1->attrib;
	top->nsubs = top1->nsubs + top2->nsubs;

	top->subelem = calloc((top1->nsubs + top2->nsubs), sizeof(struct XmElem));

	for (int i=0; i < top1->nsubs; i++) {
		(*top->subelem)[i] = (*top1->subelem)[i];
	}
	int k = 0;
	for (int i=top1->nsubs; i < (top1->nsubs + top2->nsubs); i++) {
		(*top->subelem)[i] = (*top2->subelem)[k];	//after storing top1, continue with top2
		k += 1;
	}

	int status = mxWriteFile(top, outfile);
	if (status == -1) {
		fprintf(stderr, "Error trying to write records to file\n");	
		free(top->subelem);
		free(top);
		return EXIT_FAILURE;
	}

	free(top->subelem);
	free(top);
	return EXIT_SUCCESS;
}

int selects( const XmElem *top, const enum SELECTOR sel, const char *pattern, FILE *outfile ){	
	char fs, *regex = NULL;
	int reg = 0;
	BibData bdata;
	XmElem *new;

	if (pattern[0] == '"' && pattern[strlen(pattern) - 1] == '"') {
		if ((pattern[1]!='a' || pattern[1]!='t' || pattern[1]!='p') && pattern[2]!='=') {
			fprintf(stderr, "Field and regex not correct syntax\n");
			return EXIT_FAILURE; 
		} else {
			regex = calloc(strlen(pattern) - 3, sizeof(char));
			strncpy(regex, pattern+3, strlen(pattern) - 1);
			regex[strlen(regex)] = '\0';
		}
	} else {
		if ((pattern[0]!='a' || pattern[0]!='t' || pattern[0]!='p') && pattern[1]!='=') {
			fprintf(stderr, "Field and regex not correct syntax\n");
			return EXIT_FAILURE;
		} else {
			regex = calloc(strlen(pattern) - 1, sizeof(char));
			strncpy(regex, pattern+2, strlen(pattern) - 1);
		}
	}

	fs = pattern[0];
	if (regex == NULL) {
		fprintf(stderr, "Field and regex not correct syntax\n");
		return EXIT_FAILURE;
	}

	new = calloc(1, sizeof(struct XmElem));
	new->tag = top->tag;
	new->text = top->text;
	new->isBlank = top->isBlank;
	new->nattribs = top->nattribs;
	new->attrib = top->attrib;
	new->nsubs = 0;

	new->subelem = calloc(top->nsubs, sizeof(struct XmElem));
	int j = 0;

	for (int i=0; i < top->nsubs; i++) {		//if match() returns a match and we are to keep
		marc2bib( (*top->subelem)[i], bdata);	//then point to said record
		if (fs == 'a') {						//if match() return no-match and we discard
			reg = match(bdata[AUTHOR], regex);	//then add every record that fits that category
			if (reg == 1 && sel == KEEP) {
				(*new->subelem)[j] = (*top->subelem)[i];
				j += 1;
			}
			if (reg == 0 && sel == DISCARD) {
				(*new->subelem)[j] = (*top->subelem)[i];
				j += 1;
			}
		} else if (fs == 't') {
			reg = match(bdata[TITLE], regex);
			if (reg == 1 && sel == KEEP) {
				(*new->subelem)[j] = (*top->subelem)[i];
				j += 1;
			}
			if (reg == 0 && sel == DISCARD) {
				(*new->subelem)[j] = (*top->subelem)[i];
				j += 1;
			}
		} else if (fs == 'p') {
			reg = match(bdata[PUBINFO], regex);
			if (reg == 1 && sel == KEEP) {
				(*new->subelem)[j] = (*top->subelem)[i];
				j += 1;
			}
			if (reg == 0 && sel == DISCARD) {
				(*new->subelem)[j] = (*top->subelem)[i];
				j += 1;
			}
		}
		free(bdata[TITLE]);
		free(bdata[AUTHOR]);
		free(bdata[PUBINFO]);
		free(bdata[CALLNUM]);
	}

	new->nsubs = j;
	int status = mxWriteFile(new, outfile);
	if (status == -1) {
		fprintf(stderr, "Error trying to write records to file\n");	
		free(regex);
		free(new->subelem);
		free(new);
		return EXIT_FAILURE;
	}

	free(regex);
	free(new->subelem);
	free(new);
	return EXIT_SUCCESS;
}

int libFormat( const XmElem *top, FILE *outfile ) {
	const char *keys[top->nsubs];
	XmElem *collection = NULL;
	BibData bdata;
	char *bdTemp[top->nsubs][4];

  	if (top != NULL) {
		collection = calloc(1, sizeof(XmElem));
		collection->nsubs = top->nsubs;
		for (int i = 0; i < top->nsubs; i++) {
			marc2bib( (*top->subelem)[i], bdata);
			if (bdata != NULL) {
				bdTemp[i][0] = bdata[TITLE];	//store bibdata values in keys[i] to be passed
				bdTemp[i][1] = bdata[AUTHOR];	//into sortRecs to be sorted
				bdTemp[i][2] = bdata[PUBINFO];
				bdTemp[i][3] = bdata[CALLNUM];
				keys[i] = bdata[CALLNUM];
			} else {
				return EXIT_FAILURE;
			}
		}

		sortRecs(collection, keys);
		for (int i = 0; i < collection->nsubs; i++) {		//loops through every record 
			for (int j = 0; j < top->nsubs; j++) {			//and prints in the order that keys
				marc2bib( (*top->subelem)[j], bdata);		//is ordered in
				if (bdata != NULL) {
					if (strcmp(keys[i], bdata[CALLNUM]) == 0) {
						char *author, *title, *pubinfo, *callnum;
						author = bdata[AUTHOR];
						title = bdata[PUBINFO];
						pubinfo = bdata[PUBINFO];	
						callnum = bdata[CALLNUM];
						if (callnum[strlen(callnum) - 1] == '.')
							fprintf(outfile, "%s ", bdata[CALLNUM]);
						else
							fprintf(outfile, "%s. ", bdata[CALLNUM]);
						if (author[strlen(author) - 1] == '.')
							fprintf(outfile, "%s ", bdata[AUTHOR]);
						else
							fprintf(outfile, "%s. ", bdata[AUTHOR]);
						if (title[strlen(title) - 1] == '.')
							fprintf(outfile, "%s ", bdata[TITLE]);
						else
							fprintf(outfile, "%s. ", bdata[TITLE]);
						if (pubinfo[strlen(pubinfo) - 1] == '.')
							fprintf(outfile, "%s\n\n", bdata[PUBINFO]);
						else
							fprintf(outfile, "%s.\n\n", bdata[PUBINFO]);
					}
					free(bdata[CALLNUM]);
					free(bdata[AUTHOR]);
					free(bdata[TITLE]);
					free(bdata[PUBINFO]);
				} else {
					return EXIT_FAILURE;
				}
			}
		}

		free(collection);
		for (int i = 0; i < top->nsubs; i++) {
			free(bdTemp[i][0]);
			free(bdTemp[i][1]);
			free(bdTemp[i][2]);
			free(bdTemp[i][3]); 
		}
		return EXIT_SUCCESS;
	} else {
		return EXIT_FAILURE;
	}
}

int bibFormat( const XmElem *top, FILE *outfile ){
	XmElem *collection = NULL;
	BibData bdata;
	const char *keys[top->nsubs];
	char *bdTemp[top->nsubs][4];
	int repeatCounter = -1;

  	if (top != NULL) {
		collection = calloc(1, sizeof(XmElem));
		collection->nsubs = top->nsubs;
		for (int i = 0; i < top->nsubs; i++) {
			marc2bib( (*top->subelem)[i], bdata);
			if (bdata != NULL) {
				bdTemp[i][0] = bdata[TITLE];
				bdTemp[i][1] = bdata[AUTHOR];
				bdTemp[i][2] = bdata[PUBINFO];
				bdTemp[i][3] = bdata[CALLNUM];
				keys[i] = bdata[AUTHOR];
			} else {
				return EXIT_FAILURE;
			}
		}

		sortRecs(collection, keys);
		for (int i = 0; i < collection->nsubs; i++) { 	//loop through collection 
			repeatCounter = -1;
			for (int j = 0; j < top->nsubs; j++) {
				marc2bib( (*top->subelem)[j], bdata);	//loop thru rec to find right author
				if (bdata != NULL) {
					if (strcmp(keys[i], bdata[AUTHOR]) == 0) { 
						fprintf(outfile, "%s. ", bdata[AUTHOR]);
						fprintf(outfile, "%s. ", bdata[TITLE]);
						fprintf(outfile, "%s\n\n", bdata[PUBINFO]);
						repeatCounter += 1;				//keeps track of repeat authors
					}
					free(bdata[CALLNUM]);
					free(bdata[AUTHOR]);
					free(bdata[TITLE]);
					free(bdata[PUBINFO]);
				} else {
					return EXIT_FAILURE;
				}
				if (repeatCounter > 0) {
					i = i + repeatCounter;	//tells keys to skip x records because same author
				}
			}
		}

		free(collection);
		for (int i = 0; i < top->nsubs; i++) {
			free(bdTemp[i][0]);
			free(bdTemp[i][1]);
			free(bdTemp[i][2]);
			free(bdTemp[i][3]); 
		}
		return EXIT_SUCCESS;
	} else {
		return EXIT_FAILURE;
	}
}

//helper functions
int match( const char *data, const char *regex ){
	regex_t reg;
	int reti;

	reti = regcomp(&reg, regex, REG_EXTENDED);
	if (reti != 0) {
		fprintf(stderr, "Could not compile regex\n");
		return 0;
	}

	reti = regexec(&reg, data, 0, NULL, 0);
	regfree(&reg);
	if (!reti) { //MATCH
		return 1;
	} else {
		return 0;
	}
}

void marc2bib( const XmElem *mrec, BibData bdata ){
	int aField = 0, aSubfield = 0;
	int tField = 0, tSubfield = 0; 
	int pField = 0, pSubfield = 0;
	int cField = 0, cSubfield = 0;
	const char *temp;
	char *t1, *t2, *t3;
	char *author, *title, *pubinfo, *callnum;

	t1 = calloc(1 + 1, sizeof(char));
	t2 = calloc(1 + 1, sizeof(char));
	t3 = calloc(1 + 1, sizeof(char));
	strcpy(t1, "");
	strcpy(t2, "");
	strcpy(t3, "");

	//finds author from record
	aField = mxFindField(mrec, 100);	//confirms that there is said author in record
	if (aField == 0)
		aField = mxFindField(mrec, 130);
	if (aField > 0) {
		aSubfield = mxFindSubfield(mrec, 100, 1, 'a');
		if (aSubfield == 1) {
			temp = mxGetData(mrec, 100, 1, 'a', 1);		//retrives data and stores in bibdata
			author = calloc(strlen(temp) + 1, sizeof(char));
			strcpy(author, temp);
			bdata[AUTHOR] = author;
		}
	} else {
		author = calloc(3, sizeof(char));				//if there is no correct data
		strcpy(author, "na");							//just store na
		bdata[AUTHOR] = author;
	}

	//finds title from record
	tField = mxFindField(mrec, 245);
	if (tField > 0) {
		tSubfield = mxFindSubfield(mrec, 245, 1, 'a');	//retrieves data from each subfield
		if (tSubfield == 1) {
			temp = mxGetData(mrec, 245, 1, 'a', 1);
			t1 = realloc(t1, strlen(temp) + 1);
			strcpy(t1, temp);
		}
		tSubfield = mxFindSubfield(mrec, 245, 1, 'p');
		if (tSubfield == 1) {
			temp = mxGetData(mrec, 245, 1, 'p', 1);
			t2 = realloc(t2, strlen(temp) + 1);
			strcpy(t2, temp);
		}		
		tSubfield = mxFindSubfield(mrec, 245, 1, 'b');
		if (tSubfield == 1) {
			temp = mxGetData(mrec, 245, 1, 'b', 1);
			t3 = realloc(t3, strlen(temp) + 1);
			strcpy(t3, temp);
		}
		title = calloc(strlen(t1) + strlen(t2) + strlen(t3) + 1, sizeof(char));
		strcpy(title, t1);				//catinate the 3 fields - if one of more of the fields
		title = strcat(title, t2);		//are blank it will catinate it as ""
		title = strcat(title, t3);					
		bdata[TITLE] = title;

		t1 = realloc(t1, 1 + 1);
		t2 = realloc(t2, 1 + 1);
		t3 = realloc(t3, 1 + 1);
		strcpy(t1, "");
		strcpy(t2, "");
		strcpy(t3, "");
	} else {
		//no tag found
		title = calloc(3, sizeof(char)); 
		strcpy(title, "na");
		bdata[TITLE] = title;
	}

	//finds publication info from record
	pField = mxFindField(mrec, 260);
	if (pField > 0) {
		pSubfield = mxFindSubfield(mrec, 260, 1, 'a');	//functions identically to the above
		if (pSubfield == 1) {
			temp = mxGetData(mrec, 260, 1, 'a', 1);
			t1 = realloc(t1, strlen(temp) + 1);
			strcpy(t1, temp);
		}
		pSubfield = mxFindSubfield(mrec, 260, 1, 'b');
		if (pSubfield == 1) {
			temp = mxGetData(mrec, 260, 1, 'b', 1);
			t2 = realloc(t2, strlen(temp) + 1);
			strcpy(t2, temp);
		}		
		pSubfield = mxFindSubfield(mrec, 260, 1, 'c');
		if (pSubfield == 1) {
			temp = mxGetData(mrec, 260, 1, 'c', 1);
			t3 = realloc(t3, strlen(temp) + 1);
			strcpy(t3, temp);
		}
		pubinfo = calloc(strlen(t1) + strlen(t2) + strlen(t3) + 1, sizeof(char));
		strcpy(pubinfo, t1);
		pubinfo = strcat(pubinfo, t2);
		pubinfo = strcat(pubinfo, t3);					
		bdata[PUBINFO] = pubinfo;

		t1 = realloc(t1, 1 + 1);
		t2 = realloc(t2, 1 + 1);
		t3 = realloc(t3, 1 + 1);
		strcpy(t1, "");
		strcpy(t2, "");
		strcpy(t3, "");
	} else if (mxFindField(mrec, 250) > 0) {
		pSubfield = mxFindSubfield(mrec, 260, 1, 'a');
		if (pSubfield == 1) {
			temp = mxGetData(mrec, 100, 1, 'a', 1);
			pubinfo = calloc(strlen(temp) + 1, sizeof(char));
			strcpy(pubinfo, temp);
			bdata[PUBINFO] = pubinfo;
		}
	} else {
		pubinfo = calloc(3, sizeof(char));	//no tag found
		strcpy(pubinfo, "na");
		bdata[PUBINFO] = pubinfo;
	}
	
	//finds call number for record
	cField = mxFindField(mrec, 90);
	if (cField > 0) {
		cSubfield = mxFindSubfield(mrec, 90, 1, 'a');
		if (cSubfield == 1) {
			temp = mxGetData(mrec, 90, 1, 'a', 1);
			t1 = realloc(t1, strlen(temp) + 1);
			strcpy(t1, temp);
		}
		cSubfield = mxFindSubfield(mrec, 90, 1, 'b');
		if (cSubfield == 1) {
			temp = mxGetData(mrec, 90, 1, 'b', 1);
			t2 = realloc(t2, strlen(temp) + 1);
			strcpy(t2, temp);
		}		
		callnum = calloc(strlen(t1) + strlen(t2) + 1, sizeof(char));
		strcpy(callnum, t1);
		callnum = strcat(callnum, t2);
		bdata[CALLNUM] = callnum;
	} else if (mxFindField(mrec, 50) > 0) {
		cSubfield = mxFindSubfield(mrec, 50, 1, 'a');
		if (cSubfield == 1) {
			temp = mxGetData(mrec, 50, 1, 'a', 1);
			t1 = realloc(t1, strlen(temp) + 1);
			strcpy(t1, temp);
		}
		cSubfield = mxFindSubfield(mrec, 50, 1, 'b');
		if (cSubfield == 1) {
			temp = mxGetData(mrec, 50, 1, 'b', 1);
			t2 = realloc(t2, strlen(temp) + 1);
			strcpy(t2, temp);
		}		
		callnum = calloc(strlen(t1) + strlen(t2) + 1, sizeof(char));
		strcpy(callnum, t1);
		callnum = strcat(callnum, t2);
		bdata[CALLNUM] = callnum;
	} else {
		callnum = calloc(3, sizeof(char));	//no tag found
		strcpy(callnum, "na");
		bdata[CALLNUM] = callnum;
	}
	free(t1);
	free(t2);
	free(t3);
}

void sortRecs( XmElem *collection, const char *keys[] ) {
	if (collection->nsubs > 1) {
		qsort(keys, collection->nsubs, sizeof(keys[0]), comp);
	}
}

//Comparison function for qsort - just strcmp
int comp (const void *a, const void *b) {
	return (strcmp(*(char **)a, *(char **)b));
}

