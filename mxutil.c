/********
Kevin Tran
0599585
CIS2750
Assignment 1
mxutil.c 
a library of functions that consist of parsing marcxml file with schema and storing it into
a XmElem struct for easier manipulation
 ********/
#define _POSIX_SOURCE 1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "mxutil.h"
#include <libxml/xmlschemastypes.h>

int writeHelper( const XmElem *top, FILE *mxfile );

xmlSchemaPtr mxInit( const char *xsdfile ) {
	xmlSchemaParserCtxtPtr parser = NULL;
	xmlSchemaPtr sp = NULL;
	LIBXML_TEST_VERSION
	
	parser = xmlSchemaNewParserCtxt(xsdfile);	
	if (parser == NULL) {
  		return NULL;
    }
	
	sp = xmlSchemaParse(parser);
    if (sp == NULL) {
       	return NULL;
    }

    xmlSchemaFreeParserCtxt(parser);
	xmlLineNumbersDefault(1);

	return sp;
}

int mxReadFile( FILE *marcxmlfp, xmlSchemaPtr sp, XmElem **top ) {
	xmlSchemaValidCtxtPtr validCtxt = NULL;
	xmlDocPtr doc = NULL;
	xmlNodePtr node = NULL;
	int validate = 99;

	doc = xmlReadFd(fileno(marcxmlfp), "", NULL, 0);
	if (doc == NULL) {
		*top = NULL;
		return 1;
	}

	validCtxt = xmlSchemaNewValidCtxt(sp);
	if (!validCtxt) {
		*top = NULL;
		return 2;
    }

	validate = xmlSchemaValidateDoc(validCtxt, doc);
	if (validate != 0) {	
		*top = NULL;
		return 2;
	}

	node = xmlDocGetRootElement(doc);

	if (node == NULL) { //document is empty
		*top = NULL;
		return 0;
	}

	if (doc != NULL) {
		*top = mxMakeElem(doc, node);
	} else {
		*top = NULL;
	}
	xmlFreeDoc(doc);
	xmlSchemaFreeValidCtxt(validCtxt);

	return 0;
}

XmElem *mxMakeElem( xmlDocPtr doc, xmlNodePtr node ) {
	XmElem *xmlPtr = NULL;
	xmlAttrPtr attr = NULL;
	int attribCounter = 0;

	if (node->type != XML_COMMENT_NODE) {

	xmlPtr = calloc(1, sizeof(struct XmElem)); 
	xmlPtr->tag = calloc(strlen((char *)node->name) + 1, sizeof(char));
	strcpy(xmlPtr->tag , (char *)node->name);

	xmlChar *temp;
	temp = xmlNodeListGetString(doc, node->children, 1); //gets text from tags - can be blank
	if (temp != NULL) {
		xmlPtr->text = (char *)xmlNodeListGetString(doc, node->children, 1);
	} else {
		xmlPtr->text = NULL;
	}
	xmlFree(temp);

	int blankCheck = 1, x;
	for (int i = 0; i < strlen((char*)xmlPtr->text); i++) {
		x = isspace(xmlPtr->text[i]); //checks if text is blank or not
		if (x == 0) {
			blankCheck = 0;
		}
	}
	if (blankCheck == 1) { 
		xmlPtr->isBlank = 1; //only whitespace found
	} else {		
		xmlPtr->isBlank = 0; //nonwhitespace found - flag 0
	}

	attr = node->properties;
	if (attr != NULL) {		 //loops through all attributes to find out nattribs
		while (attr != NULL) {
			attribCounter = attribCounter+1;			
			attr = attr->next;
		}
	}
	xmlPtr->nattribs = attribCounter;

	if (xmlPtr->nattribs > 0) {
		attr = node->properties;
		
		xmlPtr->attrib = calloc(xmlPtr->nattribs, sizeof(char*[2]));
		//allocates and stores attributes into their proper place [0]=name [1]=values
		for (int i = 0; i < xmlPtr->nattribs; i++) {
		 	(*xmlPtr->attrib)[i][0] = calloc(strlen((char*)attr->name) + 1, sizeof(char));
			strcpy((*xmlPtr->attrib)[i][0], (char *)attr->name);
			(*xmlPtr->attrib)[i][1] = (char *)xmlGetProp(node, attr->name);			
			attr = attr->next;	//goes to next attrib and continues loop
		}
	} else {
		xmlPtr->attrib = NULL;
	}

	xmlPtr->nsubs = xmlChildElementCount(node);
	xmlPtr->subelem = calloc(xmlPtr->nsubs, sizeof(XmElem));

	//loops through records - at a record it goes through its siblings (ie the data)
	if (xmlFirstElementChild(node)) {
		(*xmlPtr->subelem)[0] = mxMakeElem(doc, xmlFirstElementChild(node));
		if (xmlNextElementSibling((xmlFirstElementChild(node)))) {	//no more children
			node = xmlFirstElementChild(node);
			for (int j = 0; j <xmlPtr->nsubs; j++) {	//recursively send new mode to top
					(*xmlPtr->subelem)[j] = mxMakeElem(doc, node);
				node = xmlNextElementSibling(node);
			}
		}
	}
	xmlFreePropList(attr);
	}
	return xmlPtr;
}

int mxWriteFile( const XmElem *top, FILE *mxfile ) {
	int status = 0;
	fprintf(mxfile, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	fprintf(mxfile, "<!-- Output by mxutil library ( Kevin Tran ) -->\n");
	
	writeHelper(top, mxfile);
	return status;
}

//Precondition: top successfully returned from mxReadFile, and mxfile is open for writing
//Postcondition: new MARCXML file has been output containing all records in top
// Helper function to keep track of recursion a little easier
int writeHelper( const XmElem *top, FILE *mxfile ) {
	xmlChar *string;	
	int static depth=0;

	for ( int i=0; i<depth; i++ ) printf( "\t" );

	fprintf(mxfile, "<marc:%s", top->tag );
 	for ( int i=0; i < top->nattribs; i++ ) {
		if (strcmp(top->tag, "collection") == 0) {	//manually print out collection header
			fprintf(mxfile, " xmlns:marc=\"http://www.loc.gov/MARC21/slim\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:schemaLocation=\"http://www.loc.gov/MARC21/slim http://www.loc.gov/standards/marcxml/schema/MARC21slim.xsd\"");
		} else {
    	fprintf(mxfile, " %s=\"%s\"", (*top->attrib)[i][0], (*top->attrib)[i][1] );
		}
	}

	if (top->isBlank != 1) {
		//encodes the five predfined entities back to a MARCXML compliant form
		string = xmlEncodeSpecialChars(NULL, (xmlChar*)top->text);
		fprintf(mxfile, ">%s", (char *)string);
		xmlFree(string);
	} else {
		fprintf(mxfile, ">\n");
	}

	for ( int i=0; i < top->nsubs; i++ ) {
		++depth;
     	writeHelper( (*top->subelem)[i], mxfile );	
		--depth;
	}
	fprintf(mxfile, "</marc:%s>\n", top->tag);

	return top->nsubs;
}

void mxCleanElem( XmElem *top ) {
 	for ( int i=0; i < top->nsubs; i++ ) {	//recursively travels down to the lowest record
    	mxCleanElem((*top->subelem)[i]);	//frees that record's contents and then frees
		free((*top->subelem)[i]);			//itself
  	}
	free(top->tag);	
	if (top->text) {	
		xmlFree(top->text);
	}
	for ( int i=0; i < top->nattribs; i++ ) {
		free((*top->attrib)[i][0]);
		xmlFree((*top->attrib)[i][1]); 
	}
	free(top->attrib);
	free(top->subelem);
}

void mxTerm( xmlSchemaPtr sp ) {
	xmlSchemaFree(sp);
	xmlSchemaCleanupTypes();	
	xmlCleanupParser();
}

int mxFindField( const XmElem *mrecp, int tag ) {
	int nfields = 0, temp;
	XmElem *xmlPtr = NULL;
	
  	for (int i = 0; i < mrecp->nsubs; i++) {
		for (int j = 0; j < (*mrecp->subelem)[i]->nattribs; j++) {
			xmlPtr = (*mrecp->subelem)[i];
			temp = atoi((*xmlPtr->attrib)[j][1]);
			if (temp == tag)
				nfields = nfields + 1;
		}
	}	
	return nfields;
}

int mxFindSubfield( const XmElem *mrecp, int tag, int tnum, char sub ){
	int nsf = 0, temp, counter = 0;
	XmElem *xmlPtr = NULL, *xmlPtr2 = NULL;
	
  	for (int i = 0; i < mrecp->nsubs; i++) {
		for (int j = 0; j < (*mrecp->subelem)[i]->nattribs; j++) {
			xmlPtr = (*mrecp->subelem)[i];
			temp = atoi((*xmlPtr->attrib)[j][1]); //loops through tags
			if (temp == tag) {	//finds the right tag
				counter	+= 1;	//makes sure you are on the tnum'th instance
				if (counter == tnum) {
					for (int k = 0; k < xmlPtr->nsubs; k++) { //loops through chldren if any
						xmlPtr2 = (*xmlPtr->subelem)[k];
						for (int l = 0; l < xmlPtr2->nattribs; l++) { //loop through attribs
							if (strlen((*xmlPtr2->attrib)[l][1]) == 1) { 
								if (strchr((*xmlPtr2->attrib)[l][1], sub))
									nsf += 1;
							}
						}	
					}			
				} 		
			} else if (tnum > counter) {
				nsf = 0;
			}
		}
	}	
	return nsf;
}

const char *mxGetData( const XmElem *mrecp, int tag, int tnum, char sub, int snum ){
	int tempTag, counterTag = 0, counterSub = 0;
	XmElem *xmlPtr = NULL, *xmlPtr2 = NULL;
	char *data = NULL;
	

	for (int i = 0; i < mrecp->nsubs; i++) {
		for (int j = 0; j < (*mrecp->subelem)[i]->nattribs; j++) {
			xmlPtr = (*mrecp->subelem)[i];
			tempTag = atoi((*xmlPtr->attrib)[j][1]); //loops through tags
			if (tempTag == tag && tag > 10) {	//finds the right tag
				counterTag	+= 1;	//makes sure you are on the tnum'th instance
				if (counterTag == tnum) {
					for (int k = 0; k < xmlPtr->nsubs; k++) {//loops through chldren if any
						xmlPtr2 = (*xmlPtr->subelem)[k];
						for (int l = 0; l < xmlPtr2->nattribs; l++) { //loops atribs if any
							if (strchr((*xmlPtr2->attrib)[l][1], sub)) { //finds the right subf
								counterSub += 1;
								if (counterSub == snum) {
									data = xmlPtr2->text;
									//data = calloc(strlen(xmlPtr2->text) + 1, sizeof(char));
									//strcpy(data, xmlPtr2->text);
								}									
							}
						}	
					}			
				} 		
			} else if (tempTag == tag && tag >= 0 && tag <= 9) { //000-009 has no subfields
				counterTag	+= 1;	//makes sure you are on the tnum'th instance
				if (counterTag == tnum) {
					data = xmlPtr->text;
					//data = calloc(strlen(xmlPtr->text) + 1, sizeof(char));
					//strcpy(data, xmlPtr->text);
				}
			}
		}
	}	
	return data;
}


 
