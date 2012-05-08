#!/usr/bin/python
#Kevin Tran
#0599585
#CIS2750
#Assignment 4
#altro.py
#python gui that interacts with mxutil, mxtool, and mxpylib functions
#Now connects to postgresql database

from Tkinter import *
from tkFileDialog import *
import tkMessageBox
import os
import Mx
import re
import tempfile
import psycopg2

class MainWindow(Menu):
	def __init__(self, master, query=None):
		frame = Frame(master)
		frame.pack()

		self.toplevel = None
		self.query = query

		self.label = Label(master, bd=1, relief=SUNKEN, anchor=S)
		self.label.pack(side=BOTTOM, fill=X)

		self.yScroll = Scrollbar(frame, orient=VERTICAL)
		self.yScroll.grid(row=0, column=10, sticky=N+S+E+W)
		self.xScroll = Scrollbar(frame, orient=HORIZONTAL)
		self.xScroll.grid(row=1, column=0, columnspan=10, sticky=N+S+E+W)

		self.display = Listbox(frame, selectmode=EXTENDED, bg="white", yscrollcommand=self.yScroll.set, xscrollcommand=self.xScroll.set)
		self.display.grid(row=0, column=0, columnspan=10, sticky=N+S+E+W)
		self.yScroll.config(command=self.display.yview)
		self.xScroll.config(command=self.display.xview)
		self.display.bind("<Button-1>", self.listClick)

		self.delete = Button(frame, text="Delete Selected Records", state=DISABLED, command=self.delRecord)
		self.delete.grid(row=2, column=7, columnspan=3, sticky=E)

		self.var = StringVar()
		self.author = Radiobutton(frame, text="Author", variable=self.var, value='a')
		self.author.grid(row=2, column=0, sticky=W)
		self.title = Radiobutton(frame, text="Title", variable=self.var, value='t')
		self.title.grid(row=2, column=1, sticky=W)
		self.pubdata = Radiobutton(frame, text="Publisher Data", variable=self.var, value='p')
		self.pubdata.grid(row=2, column=2, sticky=W)

		self.regex = Entry(frame, width=60)
		self.regex.grid(row=3, column=1, columnspan=6)
		self.regexLabel = Label(frame, text="Regular Expression")
		self.regexLabel.grid(row=3, column=0)
	   	self.keep = Button(frame, text="Keep", command=self.keepCommand)
		self.keep.grid(row=3, column=7)
	   	self.discard = Button(frame, text="Discard", command=self.discardCommand)
		self.discard.grid(row=3, column=8)
	   	self.undo = Button(frame, text="Undo", state=DISABLED, command=self.undoCommand)
		self.undo.grid(row=3, column=9)

		Menu.__init__(self, master)

		filemenu = Menu(self, tearoff=0)
		self.add_cascade(label="File", underline=0, menu=filemenu)
		filemenu.add_command(label="Open", command=self.openCommand)
		filemenu.add_command(label="Insert", command=self.insertCommand)
		filemenu.add_command(label="Append", command=self.appendCommand)
		filemenu.add_command(label="Save As", command=self.saveasCommand)
		filemenu.add_command(label="Exit", command=self.quit)	

		printmenu = Menu(self, tearoff=0)
		self.add_cascade(label="Print", underline=0, menu=printmenu)
		printmenu.add("command", label="Library", command=self.libCommand)
		printmenu.add("command", label="Bibliography", command=self.bibCommand)
		
		self.dbmenu = Menu(self, tearoff=0)
		self.add_cascade(label="Database", underline=0, menu=self.dbmenu)
		self.dbmenu.add("command", label="Store All", state=DISABLED, command=self.storeAll)
		self.dbmenu.add("command", label="Store Selected", state=DISABLED, command=self.storeSelected)
		self.dbmenu.add("command", label="Open")
		self.dbmenu.add("command", label="Insert")
		self.dbmenu.add("command", label="Append")
		self.dbmenu.add("command", label="Purge", command=self.purge)
		self.dbmenu.add("command", label="Query", command=self.doShow)

		helpmenu = Menu(self, tearoff=0)
		self.add_cascade(label="Help", underline=0, menu=helpmenu)
		helpmenu.add("command", label="Keep/Discard Regex", command=self.regexCommand)
		helpmenu.add("command", label="About altro", command=self.about)
		self.collection = 0
		self.nrecs = 0
		self.nrecs2 = 0

	def openCommand(self):
		if (self.nrecs > 0):
			result = tkMessageBox.askokcancel("Warning", "Opening a new file will result in loss of current data")
			if result is True:
				filename = askopenfilename(filetypes=[("XML Files","*.xml")])
				if filename:
					Mx.freeFile(self.collection)	#free old collection for new file to be opened
					self.display.delete(0, END)
					#read MARCXML file and return status, C-pointer to top XmElem, and no. of records
					(self.status, self.collection, self.nrecs) = Mx.readFile(filename)
					if (self.status == 0):
						for x in range(0, self.nrecs):
							self.bibdata = Mx.marc2bib(self.collection, x) #extract 4 string from subelem[recno]
							self.tempstring = str(x+1)+self.bibdata[0]+self.bibdata[1]+self.bibdata[2]+self.bibdata[3]		
							self.display.insert(END,self.tempstring)
		elif (self.nrecs == 0):
			filename = askopenfilename(filetypes=[("XML Files","*.xml")])
			if filename:
				(self.status, self.collection, self.nrecs) = Mx.readFile(filename)
				if (self.status == 0):
					self.display.delete(0, END)
					for x in range (0, self.nrecs):
						self.bibdata = Mx.marc2bib(self.collection, x) #extract 4 string from subelem[recno]
						self.tempstring = str(x+1)+self.bibdata[0]+self.bibdata[1]+self.bibdata[2]+self.bibdata[3]		
						self.display.insert(END,self.tempstring)
		if (self.status == 0):
			self.label.config(text="Records: " +str(self.nrecs))
		else:
			self.label.config(text="Could not open XML File")
		if (self.display.size() > 0):
			self.dbmenu.entryconfig(0, state=NORMAL)

	def insertCommand(self):	
		if (self.display.size() > 0):
			temp = tempfile.NamedTemporaryFile(delete=False)	#tempfile for current display to be written to
			self.status = Mx.writeTemp(temp.file, self.collection)
			self.newfile = askopenfilename(filetypes=[("XML Files", "*.xml")])
			tempOutput = tempfile.NamedTemporaryFile(delete=False)
			temp.close()
			tempOutput.close()
			tstr = './mxtool'+' '+'-cat'+' '+temp.name+' '+'<'+' '+self.newfile+' '+'>'+' '+tempOutput.name
			os.system(tstr)	#command line execution: calls above statement
			Mx.freeFile(self.collection)
			self.status, self.collection, self.nrecs2 = Mx.readFile(tempOutput.name) #reads new file to update display
			if (self.status == 0):
				self.display.delete(0, END)
				for x in range (0, self.nrecs2):
					self.bibdata = Mx.marc2bib(self.collection, x) #extract 4 string from subelem[recno]
					self.tempstring = str(x+1)+self.bibdata[0]+self.bibdata[1]+self.bibdata[2]+self.bibdata[3]		
					self.display.insert(END,self.tempstring)
				self.label.config(text="Records Added: "+str(self.nrecs2-self.nrecs)+ " Total Records: " +str(self.nrecs2))
			else:
				self.label.config(text="Could not concatenate XML Files")
			os.unlink(temp.name)		#removes temporary files now incase user decides to insert multiple times
			os.unlink(tempOutput.name)	#in one session so as to not lose track of all files being created
		else:
			tkMessageBox.showerror("Error", "No XML File Open")

	def appendCommand(self):	#exactly the same as above except switches order of files being concat'd
		if (self.display.size() > 0):
			temp = tempfile.NamedTemporaryFile(delete=False)
			self.status = Mx.writeTemp(temp.file, self.collection)
			self.newfile = askopenfilename(filetypes=[("XML Files", "*.xml")])
			tempOutput = tempfile.NamedTemporaryFile(delete=False)
			temp.close()
			tempOutput.close()
			tstr = './mxtool'+' '+'-cat'+' '+self.newfile+' '+'<'+' '+temp.name+' '+'>'+' '+tempOutput.name
			os.system(tstr)
			Mx.freeFile(self.collection)
			self.status, self.collection, self.nrecs2 = Mx.readFile(tempOutput.name)
			if (self.status == 0):
				self.display.delete(0, END)
				for x in range (0, self.nrecs2):
					self.bibdata = Mx.marc2bib(self.collection, x) #extract 4 string from subelem[recno]
					self.tempstring = str(x+1)+self.bibdata[0]+self.bibdata[1]+self.bibdata[2]+self.bibdata[3]		
					self.display.insert(END,self.tempstring)
				self.label.config(text="Records Added: "+str(self.nrecs2-self.nrecs)+ " Total Records: " +str(self.nrecs2))
			else:
				self.label.config(text="Could not concatenate XML Files")
			os.unlink(temp.name)
			os.unlink(tempOutput.name)
		else:
			tkMessageBox.showerror("Error", "No XML File Open")

	def saveasCommand(self):
		if (self.display.size() > 0):
			newfile = asksaveasfilename(defaultextension=".xml")
			self.status = Mx.writeFile(newfile, self.collection, self.index)
			if (self.nrecs2 != 0):
				self.label.config(text="Records Saved: " +str(self.nrecs2))
			else:
				self.label.config(text="Records Saved: "+str(self.nrecs))
		else:
			tkMessageBox.showerror("Error", "Display is empty - No XML File is open")
		
	def quit(self):
		self.result = tkMessageBox.askyesno("Warning", "Do you wish to exit?")
		if self.result is True:
			if (self.collection != 0):
				Mx.freeFile(self.collection)
			Mx.term()	#terminate libxml2 processing and release storage
			#os.system("make clean")
			cursor.close()
			conn.close()
			root
			root.destroy()

	def libCommand(self):
		if (self.display.size() > 0):
			self.newfile = asksaveasfilename(defaultextension=".txt")
			temp = tempfile.NamedTemporaryFile(delete=False)
			self.status = Mx.writeTemp(temp.file, self.collection)
			temp.close()
			tstr = './mxtool'+' '+'-lib'+' '+'<'+' '+temp.name+' '+'>'+' '+self.newfile
			os.system(tstr)
			if (self.nrecs2 != 0):
				self.label.config(text="Records saved as -lib format: " +str(self.nrecs2))
			else:
				self.label.config(text="Records saved as -lib format: "+str(self.nrecs))
			os.unlink(temp.name)
		else:
			tkMessageBox.showerror("Error", "No XML File Open")

	def bibCommand(self):
		if (self.display.size() > 0):
			self.newfile = asksaveasfilename(defaultextension=".txt")
			temp = tempfile.NamedTemporaryFile(delete=False)
			self.status = Mx.writeTemp(temp.file, self.collection)
			temp.close()
			tstr = './mxtool'+' '+'-bib'+' '+'<'+' '+temp.name+' '+'>'+' '+self.newfile
			os.system(tstr)
			if (self.nrecs2 != 0):
				self.label.config(text="Records saved as -lib format: " +str(self.nrecs2))
			else:
				self.label.config(text="Records saved as -lib format: "+str(self.nrecs))
			os.unlink(temp.name)
		else:
			tkMessageBox.showerror("Error", "No XML File Open")

	def regexCommand(self):
		tkMessageBox.showinfo("Regex Help", "Regular Expression Examples:\n\nIf you type in 'Charles' and select the title button, the display will only show results that have Charles in the title.\n\nIf you type '(Charles|White)' and select the title button, the display will show results that have either Charles or White in the title.\n\nIf you type 200[0-9] and choose Publisher Data, the display will show all results that were published from 2000 - 2009.")

	def keepCommand(self):
		if self.regex.get() and self.var.get():
			self.regexString = '"'+str(self.var.get())+'='+self.regex.get()+'"' #creates regex from GUI
			if (self.display.size() > 0):
				temp = tempfile.NamedTemporaryFile(delete=False)	#standard tempfile cmd line steps
				self.status = Mx.writeTemp(temp.file, self.collection)
				tempOutput = tempfile.NamedTemporaryFile(delete=False)
				temp.close()
				tempOutput.close()
				tstr = './mxtool'+' '+'-keep'+' '+self.regexString+' '+'<'+' '+temp.name+' '+'>'+' '+tempOutput.name
				os.system(tstr)
				self.tempCollection = self.collection	#create a reference to old collection incase of undo
				self.status, self.collection, self.nrecs2 = Mx.readFile(tempOutput.name)
				if (self.status == 0):
					self.display.delete(0, END)
					for x in range (0, self.nrecs2):
						self.bibdata = Mx.marc2bib(self.collection, x) #extract 4 string from subelem[recno]
						self.tempstring = str(x+1)+self.bibdata[0]+self.bibdata[1]+self.bibdata[2]+self.bibdata[3]		
						self.display.insert(END,self.tempstring)
					self.label.config(text="Records Remaining: "+str(self.nrecs2))
				else:
					self.label.config(text="Could not carry out regex operation")
				os.unlink(temp.name)
				os.unlink(tempOutput.name)
				self.undo.config(state=NORMAL)
			else:
				tkMessageBox.showerror("Error", "No XML File Open")
		else:
			tkMessageBox.showerror("Warning", "Make sure it is a proper regular expression and one of the buttons has been selected")

	def discardCommand(self):
		if self.regex.get() and self.var.get():
			self.regexString = '"'+str(self.var.get())+'='+self.regex.get()+'"'
			if (self.display.size() > 0):
				temp = tempfile.NamedTemporaryFile(delete=False)
				self.status = Mx.writeTemp(temp.file, self.collection)
				tempOutput = tempfile.NamedTemporaryFile(delete=False)
				temp.close()
				tempOutput.close()
				tstr = './mxtool'+' '+'-discard'+' '+self.regexString+' '+'<'+' '+temp.name+' '+'>'+' '+tempOutput.name
				os.system(tstr)
				self.tempCollection = self.collection
				self.status, self.collection, self.nrecs2 = Mx.readFile(tempOutput.name)
				if (self.status == 0):
					self.display.delete(0, END)
					for x in range (0, self.nrecs2):
						self.bibdata = Mx.marc2bib(self.collection, x)
						self.tempstring = str(x+1)+self.bibdata[0]+self.bibdata[1]+self.bibdata[2]+self.bibdata[3]		
						self.display.insert(END,self.tempstring)
					self.label.config(text="Records Remaining: "+str(self.nrecs2))
				else:
					self.label.config(text="Could not carry out regex operation")
				os.unlink(temp.name)
				os.unlink(tempOutput.name)
				self.undo.config(state=NORMAL)
			else:
				tkMessageBox.showerror("Error", "No XML File Open")
		else:
			tkMessageBox.showerror("Warning", "Make sure it is a proper regular expression and one of the buttons has been selected")

	def about(self):
		tkMessageBox.showinfo("About", "Altro developed by Kevin Tran.\nFully compatible with MARC21slim XML Schema")

	def delRecord(self):
		#loop through nrecs - and if reclist = nrecs don't keep that record
		self.index = self.display.curselection()
		#self.tempCollection = self.collection	#this would work if delete actually deleted properly
		for x in range (len(self.index), 0, -1):
			self.display.delete(self.index[x-1])
		self.undo.config(state=NORMAL)

	def undoCommand(self):
		self.display.delete(0, END)
		Mx.freeFile(self.collection)	#frees collection because the user wishes to undo
		self.collection = self.tempCollection	#have the empty collection point to the old collection
		for x in range (0, self.nrecs):	#update display with old data		
			self.bibdata = Mx.marc2bib(self.collection, x)
			self.tempstring = str(x+1)+self.bibdata[0]+self.bibdata[1]+self.bibdata[2]+self.bibdata[3]		
			self.display.insert(END,self.tempstring)
		self.nrecs2 = self.nrecs
		self.label.config(text="Records: "+str(self.nrecs2))
		self.undo.config(state=DISABLED)	#disable undo key to not be abused and not crash

	def listClick(self, event):	#enables "Delete Selected Records" button when something in list is clicked
		self.delete.config(state=NORMAL)
		self.dbmenu.entryconfig(1, state=NORMAL)

	def storeAll(self):
		self.uniqueInserts = 0
		for x in range (0, self.nrecs):	#loops through each record
			self.bibdata = Mx.marc2bib(self.collection, x)	#extracts data
			self.xmlText = Mx.makeElem(self.collection, x)	#gets xml text for corresponding xml element
			self.year = re.search("([0-9][0-9][0-9][0-9])", self.bibdata[2])	#regex to extract just the year
			#0 = title, 1 = author, 2 = pubinfo, 3 = callnum - date should be in 2
			cursor.execute("SELECT * FROM bibrec WHERE title=%s and author=%s", (self.bibdata[0], self.bibdata[1]))
			if (cursor.rowcount == 0):	
				self.uniqueInserts += 1
				if self.year is None:
					self.nYear = "NULL"
					cursor.execute("INSERT INTO bibrec (author, title, pubinfo, callnum, year, xml) VALUES (%s, %s, %s, %s, %s, %s)", (self.bibdata[1], self.bibdata[0], self.bibdata[2], self.bibdata[3], self.nYear, self.xmlText))
				else:
					cursor.execute("INSERT INTO bibrec (author, title, pubinfo, callnum, year, xml) VALUES (%s, %s, %s, %s, %s, %s)", (self.bibdata[1], self.bibdata[0], self.bibdata[2], self.bibdata[3], self.year.group(), self.xmlText))
		self.label.config(text="Records inserted to DB: "+str(self.uniqueInserts))

	def storeSelected(self):
		self.uniqueInserts = 0
		self.index = self.display.curselection()	#gets the index of elements user wishes to keep
		for x in range (0, len(self.index)):
			self.bibdata = Mx.marc2bib(self.collection, int(self.index[x]))	#extracts data
			self.xmlText = Mx.makeElem(self.collection, int(self.index[x]))	#gets xml text for corresponding xml element
			self.year = re.search("([0-9][0-9][0-9][0-9])", self.bibdata[2])	#regex to extract just the year
			cursor.execute("SELECT * FROM bibrec WHERE title=%s and author=%s", (self.bibdata[0], self.bibdata[1]))
			if (cursor.rowcount == 0):	
				self.uniqueInserts += 1
				if self.year is None:
					self.nYear = "NULL"
					cursor.execute("INSERT INTO bibrec (author, title, pubinfo, callnum, year, xml) VALUES (%s, %s, %s, %s, %s, %s)", (self.bibdata[1], self.bibdata[0], self.bibdata[2], self.bibdata[3], self.nYear, self.xmlText))
				else:
					cursor.execute("INSERT INTO bibrec (author, title, pubinfo, callnum, year, xml) VALUES (%s, %s, %s, %s, %s, %s)", (self.bibdata[1], self.bibdata[0], self.bibdata[2], self.bibdata[3], self.year.group(), self.xmlText))
		self.label.config(text="Records inserted to DB: "+str(self.uniqueInserts))

	def dbOpen(self):
		#when you execute store all or store selected - store a single-xml text - that is it has <collection blah blah
		#so when we choose open from the database menu, it creates a bunch of temp files and appends/inserts them until
		#all records are written to the display - tedious but works
		if (self.display.size() > 0):
			result = tkMessageBox.askokcancel("Warning", "Open will delete all data currently on the display. Are you sure?")
	
	def purge(self):
		result = tkMessageBox.askokcancel("Warning", "This will delete all data from the database table. Are you sure?")
		if result is True:
			cursor.execute("DELETE from bibrec")
		self.label.config(text="Database Purged")

	def doShow(self):
		try: self.query.deiconify()
		except:
			self.sub = QueryWindow(self.parent)
			self.sub.withdraw()
			self.sub.deiconify()
		#when toplevel is destroyed - set self.toplevel = none again
		#also use focus_set or take_focus or w/e to set focus on a textbook (select box)

class QueryWindow(Toplevel):
	def __init__(self, master):
		Toplevel.__init__(self, master)
		self.protocol('WM_DELETE_WINDOW', self.doClose)
	
		self.label1 = Label(self, text="Find all books by author ")
		self.enterAuthor = Entry(self, width=13)
		self.labelEx = Label(self, text="Ex. Jones, Sam, Gardner")
		self.label1.grid(row=0, column=1, columnspan=1, sticky=W)
		self.labelEx.grid(row=0, column=3, sticky=W)
		self.enterAuthor.grid(row=0, column=2, sticky=W)

		self.label2 = Label(self, text="How many books were published in or after ")
		self.enterYear = Entry(self, width=13)
		self.enterYear.grid(row=1, column=2, sticky=W)
		self.label2.grid(row=1, column=1, sticky=W)

		self.label3 = Label(self, text="Find all books that were published in or after ")
		self.label3.grid(row=2, column=1, sticky=W)
		self.enterX = Entry(self, width=13)
		self.enterX.grid(row=2, column=2, sticky=W)
		self.labelr = Label(self, text="(Ordered by author name)")
		self.labelr.grid(row=2, column=3, sticky=W)

		self.label4 = Label(self, text="Find all books that were published in the year  ")
		self.label4.grid(row=3, column=1, sticky=W)
		self.enterY = Entry(self, width=13)
		self.enterY.grid(row=3, column=2, sticky=W)

		self.sqlBox = Entry(self, width=40)
		self.sqlBox.insert(0, "SELECT ")
		self.sqlBox.grid(row=4, column=1, columnspan=10, sticky=N+S+E+W)

		self.help = Button(self, text="Help", command=self.helpWindow)
		self.help.grid(row=5, column=9, columnspan=1, sticky=E)
		self.submit = Button(self, text="Submit", command=self.submitQuery)
		self.submit.grid(row=5, column=8, sticky=E)

		self.yScroll = Scrollbar(self, orient=VERTICAL)
		self.yScroll.grid(row=6, column=10, sticky=N+S+E+W)
		self.xScroll = Scrollbar(self, orient=HORIZONTAL)
		self.xScroll.grid(row=7, column=0, columnspan=10, sticky=N+S+E+W)

		self.results = Listbox(self, selectmode=EXTENDED, bg="white", yscrollcommand=self.yScroll.set, xscrollcommand=self.xScroll.set)
		self.results.grid(row=6, column=0, columnspan=10, sticky=N+S+E+W)
		self.yScroll.config(command=self.results.yview(END))
		self.xScroll.config(command=self.results.xview)

		self.clear = Button(self, text="Clear", command=self.clearResults)
		self.clear.grid(row=8, column=9, columnspan=1, sticky=E)

		self.var = IntVar()
		self.one = Radiobutton(self, variable=self.var, value=1)
		self.one.grid(row=0, column=0, sticky=W)
		self.two = Radiobutton(self, variable=self.var, value=2)
		self.two.grid(row=1, column=0, sticky=W)
		self.three = Radiobutton(self, variable=self.var, value=3)
		self.three.grid(row=2, column=0, sticky=W)
		self.four = Radiobutton(self, variable=self.var, value=4)
		self.four.grid(row=3, column=0, sticky=W)
		self.five = Radiobutton(self, variable=self.var, value=5)
		self.five.grid(row=4, column=0, sticky=W)

		self.withdraw()

	def doClose(self):
		self.withdraw()

	def helpWindow(self):
		tkMessageBox.showinfo("Help", "Database Table Name: bibrec\n\n rec_id | author | title | pubinfo | callnum | year | xml\n\nSELECT <column name> FROM bibrec WHERE <column name>=<value>\nSELECT <column name> will give you the information you want, and WHERE <column name>=<value> is the condition.\n")

	def canned1(self):
		cursor.execute("SELECT * FROM bibrec WHERE author='"+self.enterAuthor.get()+"'")
		for record in cursor:
			self.results.insert(END,record)
		self.results.insert(END, "  --- --- --- --- ---")
		self.results.yview(END)

	def submitQuery(self):
		if (self.var.get() == 1):
			if "%" in self.enterAuthor.get():
				cursor.execute("SELECT * FROM bibrec WHERE author LIKE '"+self.enterAuthor.get()+"'")
			else:
				cursor.execute("SELECT * FROM bibrec WHERE author='"+self.enterAuthor.get()+"'")
		elif (self.var.get() == 2):
			cursor.execute("SELECT COUNT(YEAR) FROM bibrec WHERE year>='"+self.enterYear.get()+"'")
		elif (self.var.get() == 3):
			cursor.execute("SELECT * FROM bibrec WHERE year>='"+self.enterX.get()+"' ORDER by author")
		elif (self.var.get() == 4):
			cursor.execute("SELECT * FROM bibrec WHERE year='"+self.enterY.get()+"'")
		elif (self.var.get() == 5):
			cursor.execute(self.sqlBox.get())
		else:
			print "Select a transaction"
		for record in cursor:
			self.results.insert(END,record)
		self.results.insert(END, "  --- --- --- --- ---")
		self.results.yview(END)

	def clearResults(self):
		self.results.delete(0, END)

dbConnect = 0
if (len(sys.argv) == 4):
	#conn_string = "host='localhost' password='0599585' user="+sys.argv[1] 
	conn_string = "host='db.socs.uoguelph.ca' user="+sys.argv[1]
	try:
		conn = psycopg2.connect(conn_string)
		cursor = conn.cursor()
		dbConnect = 1
	except:
	  print "Could Not Connect to Database"
elif (len(sys.argv) == 3):
	#conn_string = "host='localhost' password='0599585' user="+sys.argv[1] 
	conn_string = "host='db.socs.uoguelph.ca' user="+sys.argv[1]
	try:
		conn = psycopg2.connect(conn_string)
		cursor = conn.cursor()
		dbConnect = 1
	except:
	  print "Could Not Connect to Database"
elif (len(sys.argv) == 2):
	#conn_string = "host='localhost' password='0599585' user="+sys.argv[1] 
	conn_string = "host='db.socs.uoguelph.ca' user="+sys.argv[1]
	try:
		conn = psycopg2.connect(conn_string)
		cursor = conn.cursor()
		dbConnect = 1
	except:
	  print "Could Not Connect to Database"
else:
	print "No Username Provided"

if (dbConnect == 1):
	root = Tk()
	root.minsize(width=740, height=270)
	root.title(string="Altro")
	root.option_add('*font', 'Helvetica -12')
	m = MainWindow(root, QueryWindow(root))
	root.config(menu=m)

	MXTOOL_XSD = os.getenv("MXTOOL_XSD")
	while True:
		status = Mx.init()	#initilize libxml2
		if (status != 0):
			result = tkMessageBox.askyesno("Warning!", "No XSD environment variable found. Do you wish to set a variable?", icon="warning")
			if result is True:
				schema = askopenfilename(filetypes=[("XSD Files",".xsd")])
				os.putenv("MXTOOL_XSD", schema)
				os.system("make mxtool")
			else: 
				break
		else:
			break
	if (status != 0):
		quit()
		
	#try: create table bibrec - will throw exception if table is already created make sure to commit
	conn.set_isolation_level(psycopg2.extensions.ISOLATION_LEVEL_AUTOCOMMIT)
	try:
		cursor.execute("CREATE TABLE bibrec (rec_id serial PRIMARY KEY, author varchar(60), title varchar(120), pubinfo text, callnum varchar(30), year smallint, xml text);")
	except:
		nothing = 0

	root.mainloop()
else:
	quit()

