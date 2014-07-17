//
//  main.cpp
//
//  Created by Denis Stadniczuk on 23/11/13.
//  Copyright (c) 2013 Denis Stadniczuk. All rights reserved.
//

#include <iostream>
#include <fstream>
#include <string>

#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"

//Shannon entropy is a measure of how much information a series of values hold.
//If all the values have the same value, the Shannon entropy will be 0
//If the values span a wide range of numbers, the Shannon entropy will be high
float computeShannonEntropy(cv::Mat gray)
{
   uchar histo[256];
   for (int i = 0; i < 256; i++) {
      histo[i] = 0;
   }

   int total = 0;
   for (int i = 0; i < gray.rows; i++) {
      for (int j = 0; j < gray.cols; j++) {
         histo[gray.row(i).col(j).ptr()[0]]++;
         total++;
      }
   }
   float entropy = 0;
   for (int i = 0; i < 256; i++) {
      if (histo[i] != (uchar)0) {
         entropy += ((float)histo[i] / (float)total) * log2f((float) histo[i] / (float)total);
      }
   }
   return -entropy;
}

int colorDifference(int ar, int ag, int ab, int br, int bg, int bb)
{
   ar = br-ar;
   ag = bg-ag;
   ab = bb-ab;
   return ar*ar + ag*ag + ab*ab;
}

int main(int argc, const char * argv[])
{
   if (argc < 2) {
//      std::cout << argv[0] << std::endl;
      std::cout << "Usage: pdfconv pdfname.pdf" << std::endl;
      return 0;
   }


   std::string pdfName = argv[1];
   
   //convert the pdf into an image to work with
   std::string cmd = "/opt/local/bin/gs -dNOPAUSE -sDEVICE=jpeg -dFirstPage=1 -dLastPage=1 -sOutputFile=tmp.jpg -dJPEGQ=100 -q "+pdfName+" -c quit";
   system(cmd.c_str());


   cv::Mat src = cv::imread("tmp.jpg", CV_LOAD_IMAGE_COLOR);
   cv::Mat gray;
   cvtColor(src,gray,CV_RGB2GRAY);

   cv::Mat work = gray.clone();

   int rows = src.rows;
   int columns = src.cols;

   cv::Mat horizontalSeps = cv::Mat::ones(rows, columns, CV_8U);
   cv::Mat verticalSeps = cv::Mat::ones(rows, columns, CV_8U);

   //Let's make the contrast very high. Each value below 200 is treated as black, everything above as white
   for (int i = 0; i < rows; i++) {
      for (int j = 0; j < columns; j++) {
         if (work.row(i).col(j).ptr()[0] < 200) {
            work.row(i).col(j).ptr()[0] = 0;
         } else {
            work.row(i).col(j).ptr()[0] = 255;
         }
         horizontalSeps.row(i).col(j) = 255;
         verticalSeps.row(i).col(j) = 255;
      }
   }

   int tableSeparatorLength_pixels = 100;

   //We check the Shannon entropy of patches of the image. If it is 0 and also black, this might be a border
   //Start with patches from left to right
   for (int r = 0; r < rows; r++) {
      for (int c = 0; c < columns - tableSeparatorLength_pixels; c+=tableSeparatorLength_pixels/10) {
         cv::Mat separatorGray(work, cv::Rect(c, r, tableSeparatorLength_pixels, 1));

         float entropy = computeShannonEntropy(separatorGray);
         if (entropy < 0.01f) {
            int val = separatorGray.row(0).col(0).ptr()[0];
            if (val == 0) {
               for (int i = 0; i < tableSeparatorLength_pixels; i++) {
                  horizontalSeps.row(r).col(c+i).ptr()[0] = 0;
               }
            }
         }
      }
   }

   tableSeparatorLength_pixels = 50;

  //now patches from top to bottom
   for (int c = 0; c < columns; c++) {
      for (int r = 0; r < rows - tableSeparatorLength_pixels; r+=tableSeparatorLength_pixels/10) {
         cv::Mat separatorGray(work, cv::Rect(c, r, 1, tableSeparatorLength_pixels));

         float entropy = computeShannonEntropy(separatorGray);
         if (entropy < 0.01f) {
            int val = separatorGray.row(0).col(0).ptr()[0];
            if (val == 0) {
               for (int i = 0; i < tableSeparatorLength_pixels; i++) {
                  verticalSeps.row(r+i).col(c).ptr()[0] = 0;
               }
            }
         }
      }
   }

//Debug
//   cv::imwrite("/Users/denis/SourceCodes/pdfTableExtract/c_out_hor.tiff", horizontalSeps);
//   cv::imwrite("/Users/denis/SourceCodes/pdfTableExtract/c_out_ver.tiff", verticalSeps);

   cv::Mat table = horizontalSeps.mul(verticalSeps);
//   cv::imwrite("/Users/denis/SourceCodes/pdfTableExtract/c_out_table.tiff", table);

   
   //Let's determine where the table borders are
   int rowSepLocs[rows];
   for (int r = 0; r < rows; r++) {
      rowSepLocs[r] = 0;
      for (int c = 0; c < columns; c++) {
         rowSepLocs[r] += horizontalSeps.row(r).col(c).ptr()[0];
      }
      rowSepLocs[r] /= columns;
   }

   int colSepLocs[columns];
   for (int c = 0; c < columns; c++) {
      colSepLocs[c] = 0;
      for (int r = 0; r < rows; r++) {
         colSepLocs[c] += verticalSeps.row(r).col(c).ptr()[0];
      }
      colSepLocs[c] /= rows;
   }

   int last = 255;
   std::vector<int> rowLocs;
   for (int r = 0; r < rows; r++) {
      if (last == 255 && rowSepLocs[r] != 255) {
         last = 0;
         rowLocs.push_back(r);
      }
      if (rowSepLocs[r] == 255) {
         last = 255;
      }
   }

   last = 255;
   std::vector<int> colLocs;
   for (int c = 0; c < columns; c++) {
      if (last == 255 && colSepLocs[c] != 255) {
         last = 0;
         colLocs.push_back(c);
      }
      if (colSepLocs[c] == 255) {
         last = 255;
      }
   }


   //handle cells spanning multiple columns
   std::vector<std::vector<bool>> colSpans;
   //each td of table gets one entry here. 1 is standard, x means number of spans, 0 means that a preceeding td has span > 1

   for (int r = 0; r < rowLocs.size()-1; r++) {
      std::vector<bool> row;
      for (int c = 0; c < colLocs.size()-1; c++) {

         //check if each value in area of interest is 255
         bool allWhite = true;
         for (int i = rowLocs[r]+1; i < rowLocs[r+1] && allWhite; i++) {
            for (int j = colLocs[c+1]-1; j < colLocs[c+1]+1 && allWhite; j++) {
               int val = (int) *table.row(i).col(j).ptr();

               if (val != 255) {
                  allWhite = false;
               }
            }
         }
         if (allWhite) {
//            std::cout << "colspan++  at row " << r << " col " << c << std::endl;
            row.push_back(true);
         } else {
            row.push_back(false);
         }
      }
      colSpans.push_back(row);
   }



   //handle cells spanning multiple rows
   std::vector<std::vector<bool>> rowSpans;
   //each td of table gets one entry here. 1 is standard, x means number of spans, 0 means that a preceeding td has span > 1

   for (int c = 0; c < colLocs.size()-1; c++) {
      std::vector<bool> col;
      for (int r = 0; r < rowLocs.size()-1; r++) {

         //check if each value in area of interest is 255
         bool allWhite = true;
         for (int i = rowLocs[r+1]-1; i < rowLocs[r+1]+1 && allWhite; i++) {
            for (int j = colLocs[c]+1; j < colLocs[c+1] && allWhite; j++) {
               int val = (int) *table.row(i).col(j).ptr();

               if (val != 255) {
                  allWhite = false;
               }
            }
         }
         if (allWhite) {
//            std::cout << "rowspan++  at row " << r << " col " << c << std::endl;
            col.push_back(true);
         } else {
            col.push_back(false);
         }
      }
      rowSpans.push_back(col);
   }




   //let's start writing the data
   std::string outTable = "<html><head><meta http-equiv='Content-Type' content='text/html; charset=UTF-8' /></head><style>td { border: 1px solid gray; }</style><body><table>\n";

   for (int i = 0; i < rowLocs.size()-1; i++) {
      outTable += "<tr>\n";
      for (int j = 0; j < colLocs.size()-1; j++) {
         int c_start = colLocs[j];
         int r_start = rowLocs[i];

         int colSpan = 1;
         while (j < colLocs.size()-1 && colSpans[i][j]) {
            colSpan++;
            j++;
         }
         int c_end = colLocs[j+1];
         int r_end = rowLocs[i+1];
         int rowSpan = 1;
         if (i > 0 && rowSpans[j][i-1]) {
//            rowSpan = 0;
//            r_end = rowLocs[i];
            continue;
         } else {
            int t = j;
            while (t < rowLocs.size()-1 && rowSpans[t][i]) {
               rowSpan++;
               t++;
            }
            r_end = rowLocs[i+rowSpan];
         }

         //use pdftotext to extract the text. Writing it to tmp.txt (A little dirty, done is better than perfect)
         std::string str = "/opt/local/bin/pdftotext " + pdfName + " -f 1 -l 1 -x " + std::to_string(c_start) + " -y " + std::to_string(r_start) + " -W " + std::to_string(c_end - c_start) + " -H " + std::to_string(r_end - r_start) + " -layout -nopgbrk tmp.txt";
//         std::cout << str << std::endl;
         system(str.c_str());
         std::ifstream ifs("tmp.txt");
         std::string content( (std::istreambuf_iterator<char>(ifs) ),
                             (std::istreambuf_iterator<char>()    ) );
         outTable += "<td colspan='" + std::to_string(colSpan) + "' rowspan='" + std::to_string(rowSpan) + "'>" + content + "</td>";

      }
      outTable += "</tr>\n";
   }

   outTable += "</table></body></html>";

   std::ofstream outfile;
   outfile.open ("out.html");
   outfile << outTable;
   outfile.close();

   return 0;
}

