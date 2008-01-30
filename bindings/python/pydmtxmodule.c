/*
pydmtx - Python wrapper for libdmtx

Copyright (c) 2006 Dan Watson
Copyright (c) 2007 Mike Laughton

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

Contact: dcwatson@gmail.com
         mike@dragonflylogic.com
*/

/* $Id$ */

#include <string.h>
#include <Python.h>
#include <dmtx.h>

static PyObject *dmtx_encode(PyObject *self, PyObject *args, PyObject *kwargs);
static PyObject *dmtx_decode(PyObject *self, PyObject *args, PyObject *kwargs);

static PyMethodDef dmtxMethods[] = {
   { "encode",
     (PyCFunction)dmtx_encode,
     METH_VARARGS | METH_KEYWORDS,
     "Encodes data into a matrix and calls back to plot." },
   { "decode",
     (PyCFunction)dmtx_decode,
     METH_VARARGS | METH_KEYWORDS,
     "Decodes data from a matrix and returns data." },
   { NULL,
     NULL,
     0,
     NULL }
};

static PyObject *dmtx_encode(PyObject *self, PyObject *arglist, PyObject *kwargs)
{
   const unsigned char *data;
   int data_size;
   int module_size;
   int margin_size;
   int scheme;
   int shape;
   PyObject *plotter = NULL;
   PyObject *start_cb = NULL;
   PyObject *finish_cb = NULL;
   PyObject *context = Py_None;
   PyObject *args;
   DmtxEncode *encode;
   int row, col;
   DmtxPixel pixel;
   static char *kwlist[] = { "data", "data_size", "module_size", "margin_size",
                             "scheme", "shape", "plotter", "start", "finish",
                             "context", NULL };

   if(!PyArg_ParseTupleAndKeywords(arglist, kwargs, "siiiii|OOOO", kwlist,
         &data, &data_size, &module_size, &margin_size, &scheme, &shape,
         &plotter, &start_cb, &finish_cb, &context))
      return NULL;

   Py_INCREF(context);

   // plotter is required, and must be callable
   if((plotter == NULL) || !PyCallable_Check(plotter)) {
      PyErr_SetString(PyExc_TypeError, "plotter must be callable");
      return NULL;
   }

   encode = dmtxEncodeStructCreate();
   encode->moduleSize = module_size;
   encode->marginSize = margin_size;
   encode->scheme = scheme;

   dmtxEncodeDataMatrix(encode, data_size, (unsigned char *)data, shape);

   if((start_cb != NULL) && PyCallable_Check(start_cb)) {
      args = Py_BuildValue("(iiO)", encode->image.width, encode->image.height, context);
      (void)PyEval_CallObject(start_cb, args);
      Py_DECREF(args);
   }

   for(row = 0; row < encode->image.height; row++) {
      for(col = 0; col < encode->image.width; col++) {
         pixel = dmtxPixelFromImage(&encode->image, col, row);
         args = Py_BuildValue("(ii(iii)O)", col, row, pixel.R, pixel.G, pixel.B, context);
         (void)PyEval_CallObject(plotter, args);
         Py_DECREF(args);
      }
   }

   if((finish_cb != NULL) && PyCallable_Check(finish_cb)) {
      args = Py_BuildValue("(O)", context);
      (void)PyEval_CallObject(finish_cb, args);
      Py_DECREF(args);
   }

   dmtxEncodeStructDestroy(&encode);
   Py_DECREF(context);

   return Py_None;
}

static PyObject *dmtx_decode(PyObject *self, PyObject *arglist, PyObject *kwargs)
{
   int width;
   int height;
   int gap_size;
   PyObject *picker = NULL;
   PyObject *context = Py_None;
   PyObject *args;
   PyObject *output;
   DmtxDecode *decode;
   PyObject *pilPixel;
   DmtxPixel dmtxPixel;
   DmtxRegion *region;
   int x, y;
   static char *kwlist[] = { "width", "height", "gap_size", "picker", "context", NULL };

   // individual pixel picker is probably a slow way to do this
   if(!PyArg_ParseTupleAndKeywords(arglist, kwargs, "iii|OO", kwlist,
         &width, &height, &gap_size, &picker, &context))
      return NULL;

   Py_INCREF(context);

   // picker is required, and must be callable
   if((picker == NULL) || !PyCallable_Check(picker)) {
      PyErr_SetString(PyExc_TypeError, "picker must be callable");
      return NULL;
   }

   decode = dmtxDecodeStructCreate();
   dmtxImageInit(&(decode->image));

   decode->image.width = width;
   decode->image.height = height;

   decode->image.pxl = (DmtxPixel *)malloc(decode->image.width * decode->image.height * sizeof(DmtxPixel));
   if(decode->image.pxl == NULL)
      return NULL;

   // populate image with PIL image data
   for(y = 0; y < decode->image.height; y++) {
      for(x = 0; x < decode->image.width; x++) {
         args = Py_BuildValue("(iiO)", x, y, context);
         pilPixel = PyEval_CallObject(picker, args);
         Py_DECREF(args);

         if(pilPixel == NULL)
            return NULL;

         if(!PyArg_ParseTuple(pilPixel, "iii", &dmtxPixel.R, &dmtxPixel.G, &dmtxPixel.B))
            return NULL;

         decode->image.pxl[y * decode->image.width + x] = dmtxPixel;

         Py_DECREF(pilPixel);
      }
   }

   dmtxScanStartNew(decode);

   for(y = gap_size; y < decode->image.height; y += gap_size) {
      if(dmtxDecodeGetMatrixCount(decode) > 0)
         break;
      else
         dmtxScanLine(decode, DmtxDirRight, y);
   }

   region = dmtxDecodeGetMatrix(decode, 0);
   if(region == NULL)
      return Py_None;

   output = Py_BuildValue("s", region->output);
   Py_INCREF(output);

   dmtxImageDeInit(&(decode->image));
   dmtxDecodeStructDestroy(&decode);
   Py_DECREF(context);

   return output;
}

PyMODINIT_FUNC init_pydmtx(void)
{
   (void)Py_InitModule("_pydmtx", dmtxMethods);
}

int main(int argc, char *argv[])
{
   Py_SetProgramName(argv[0]);
   Py_Initialize();
   init_pydmtx();
   return 0;
}
