/********
Kevin Tran
0599585
CIS2750
Assignment 3
mxpylib.c
Wrapper functions for python and mxtool.c
********/

#include <Python.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mxutil.h"
#include "mxtool.h"
#include <libxml/xmlschemastypes.h>

static PyObject *Mx_init( PyObject *self, PyObject *args ); 
static PyObject *Mx_readFile( PyObject *self, PyObject *args );
static PyObject *Mx_marc2bib( PyObject *self, PyObject *args ); 
static PyObject *Mx_writeFile( PyObject *self, PyObject *args ); 
static PyObject *Mx_writeTemp( PyObject *self, PyObject *args ); 
static PyObject *Mx_makeElem( PyObject *self, PyObject *args ); 
static PyObject *Mx_combineElem( PyObject *self, PyObject *args );
static PyObject *Mx_freeFile( PyObject *self, PyObject *args ); 
static PyObject *Mx_term( PyObject *self, PyObject *args ); 

static PyMethodDef MxMethods[] = {
	{"init", Mx_init, METH_VARARGS},
	{"readFile", Mx_readFile, METH_VARARGS},
	{"marc2bib", Mx_marc2bib, METH_VARARGS},
	{"writeFile", Mx_writeFile, METH_VARARGS},
	{"writeTemp", Mx_writeTemp, METH_VARARGS},
	{"makeElem", Mx_makeElem, METH_VARARGS},
	{"combineElem", Mx_combineElem, METH_VARARGS},
	{"freeFile", Mx_freeFile, METH_VARARGS},
	{"term", Mx_term, METH_VARARGS},
	{NULL, NULL} 
};

void initMx( void ) { Py_InitModule("Mx", MxMethods); } 

xmlSchemaPtr sp = NULL;

static PyObject *Mx_init( PyObject *self, PyObject *args ) {
	static char *xsdfile;
	
	xsdfile = getenv("MXTOOL_XSD");
	sp = mxInit(xsdfile);
	if (sp != NULL)
		return Py_BuildValue("i", 0);
	return Py_BuildValue("i", 1);
} 

static PyObject *Mx_readFile( PyObject *self, PyObject *args ) {
	XmElem *top;
	char *filename;
	int status;
	FILE *marcFile;


	if (PyArg_ParseTuple(args, "s", &filename)) {
		marcFile = fopen(filename, "r");
		status = mxReadFile(marcFile, sp, &top);
		fclose(marcFile);
	}
	return Py_BuildValue("iki", status, top, top->nsubs);
}

static PyObject *Mx_marc2bib( PyObject *self, PyObject *args ) {
	BibData bdata;
	XmElem *collection;
	int recno;

	if (PyArg_ParseTuple(args, "ki", (unsigned long*)&collection, &recno)) {
		marc2bib((*collection->subelem)[recno], bdata);
	}

	PyObject *bibObject;
	bibObject = Py_BuildValue("ssss", bdata[TITLE],bdata[AUTHOR],bdata[PUBINFO],bdata[CALLNUM]);
	free(bdata[TITLE]);
	free(bdata[AUTHOR]);
	free(bdata[PUBINFO]);
	free(bdata[CALLNUM]);

	return bibObject;
} 

static PyObject *Mx_writeFile( PyObject *self, PyObject *args ) {
	char *filename;
	XmElem *collection;
	PyObject *reclist;
	FILE *xmlFile;
	int status;

	if (PyArg_ParseTuple(args, "skO", &filename, (unsigned long*)&collection, &reclist)) {
		xmlFile = fopen(filename, "w");
		status = mxWriteFile(collection, xmlFile);
		fclose(xmlFile);
	}

	return Py_BuildValue("i", status);
}

//this function should be almost identical to Mx_writeFile except for that it does not open a file but uses the
//already opened tempfile from python-section of program
static PyObject *Mx_writeTemp( PyObject *self, PyObject *args) {
	XmElem *collection;
	PyObject *tempfile;
	int status;
	
	if (PyArg_ParseTuple(args, "Ok", &tempfile, (unsigned long*)&collection)) {
		if (PyFile_Check(tempfile)) {	
			status = mxWriteFile(collection, PyFile_AsFile(tempfile));
		} else {
		}
	}
	return Py_BuildValue("i", status);
}

//Doesn't really call makeElem - instead it makes a single element from a collection of elements
static PyObject *Mx_makeElem( PyObject *self, PyObject *args ) {
	XmElem *collection;
	XmElem *singleXml;
	int recno;

	if (PyArg_ParseTuple(args, "ki", (unsigned long*)&collection, &recno)) {
		singleXml = (*collection->subelem)[recno];
	}
	return Py_BuildValue("k", singleXml);
}

//Change XML file in DB back into XmElem so we can use it
static PyObject *Mx_combineElem( PyObject *self, PyObject *args ) {
	XmElem *collection;
	XmElem *new;
	int recno, totalRec;

	new = calloc(1, sizeof(struct XmElem));

	if (PyArg_ParseTuple(args, "kkii", (unsigned long*)&collection, (unsigned long*)&new, &recno, &totalRec)) {
		if (collection == NULL) {
			collection = calloc(1, sizeof(struct XmElem));
			collection->tag = calloc(10 + 1, sizeof(char));
			strcpy(collection->tag , "collection");
			collection->text = calloc(1, sizeof(char));
			strcpy(collection->text, " ");
			collection->isBlank = 1;
			collection->nattribs = 1;
			collection->attrib = calloc(1, sizeof(char*[2]));
			(*collection->attrib)[0][0] = calloc(14 + 1, sizeof(char));
			strcpy((*collection->attrib)[0][0], "schemaLocation");
			(*collection->attrib)[1][0] = calloc(90, sizeof(char));
			strcpy((*collection->attrib)[1][0], "http://www.loc.gov/MARC21/slim http://www.loc.gov/standards/marcxml/schema/MARC21slim.xsd");
			collection->nsubs = 0;
			collection->subelem = calloc(totalRec, sizeof(XmElem));
		}
		if (recno <=totalRec) {
			(*collection->subelem)[recno] = new;
		}
	}
	return Py_BuildValue("ki", collection, totalRec);
}

static PyObject *Mx_freeFile( PyObject *self, PyObject *args ) {
	XmElem *collection;

	if (PyArg_ParseTuple(args, "k", (unsigned long*)&collection)) {
		mxCleanElem(collection);
	}
	return Py_None;
}

static PyObject *Mx_term( PyObject *self, PyObject *args ) {
	mxTerm(sp);
	return Py_None;
}

