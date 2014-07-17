PdfTableExtract
===============

This extracts tables from PDFs. It supports cells spanning multiple rows or columns. For results, take a look at the PDF and the HTML in this repository. The HTML table was extracted from the PDF. 

I wrote this because I needed to extract the tables of a lot of PDFs, but good tools where expensive or not working well.

This is not a very user friendly tool, but if you want me to make if easier, tell me!

You need the following things installed: ghostscript, pdftotext, opencv

Compile main.cpp, link against opencv. The programm will overwrite tmp.txt and tmp.jpg in your working directory, so make sure you don't have anything important there.
