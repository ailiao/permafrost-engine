/*
 *  This file is part of Permafrost Engine. 
 *  Copyright (C) 2018-2020 Eduard Permyakov 
 *
 *  Permafrost Engine is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Permafrost Engine is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 *  Linking this software statically or dynamically with other modules is making 
 *  a combined work based on this software. Thus, the terms and conditions of 
 *  the GNU General Public License cover the whole combination. 
 *  
 *  As a special exception, the copyright holders of Permafrost Engine give 
 *  you permission to link Permafrost Engine with independent modules to produce 
 *  an executable, regardless of the license terms of these independent 
 *  modules, and to copy and distribute the resulting executable under 
 *  terms of your choice, provided that you also meet, for each linked 
 *  independent module, the terms and conditions of the license of that 
 *  module. An independent module is a module which is not derived from 
 *  or based on Permafrost Engine. If you modify Permafrost Engine, you may 
 *  extend this exception to your version of Permafrost Engine, but you are not 
 *  obliged to do so. If you do not wish to do so, delete this exception 
 *  statement from your version.
 *
 */

#include <Python.h> /* Must be included first */

#include "public/script.h"
#include "py_ui_style.h"
#include "py_pickle.h"
#include "../lib/public/pf_nuklear.h"
#include "../lib/public/vec.h"
#include "../lib/public/SDL_vec_rwops.h"
#include "../lib/public/pf_string.h"
#include "../game/public/game.h"
#include "../event.h"
#include "../collision.h"
#include "../config.h"
#include "../main.h"
#include "../ui.h"

#include <assert.h>

#define TO_VEC2T(_nk_vec2i) ((vec2_t){_nk_vec2i.x, _nk_vec2i.y})
#define TO_VEC2I(_pf_vec2t) ((struct nk_vec2i){_pf_vec2t.x, _pf_vec2t.y})

#define CHK_TRUE(_pred, _label) do{ if(!(_pred)) goto _label; }while(0)
#define CHK_TRUE_RET(_pred)   \
    do{                       \
        if(!(_pred))          \
            return false;     \
    }while(0)

typedef struct {
    PyObject_HEAD
    char                    name[128];
    struct rect             rect; /* In virtual window coordinates */
    int                     flags;
    struct nk_style_window  style;
    int                     resize_mask;
    /* The resolution for which the position and size of the window are 
     * defined. When the physical screen resolution changes to one that is
     * not equal to this window's virtual resolution, the window bounds
     * will be transformed according to the resize mask. */
    struct nk_vec2i         virt_res;
}PyWindowObject;

VEC_TYPE(win, PyWindowObject*)
VEC_IMPL(static inline, win, PyWindowObject*)

static int       PyWindow_init(PyWindowObject *self, PyObject *args, PyObject *kwargs);
static PyObject *PyWindow_layout_row_static(PyWindowObject *self, PyObject *args);
static PyObject *PyWindow_layout_row_dynamic(PyWindowObject *self, PyObject *args);
static PyObject *PyWindow_layout_row(PyWindowObject *self, PyObject *args);
static PyObject *PyWindow_layout_row_begin(PyWindowObject *self, PyObject *args);
static PyObject *PyWindow_layout_row_end(PyWindowObject *self);
static PyObject *PyWindow_layout_row_push(PyWindowObject *self, PyObject *args);
static PyObject *PyWindow_label_colored(PyWindowObject *self, PyObject *args);
static PyObject *PyWindow_label_colored_wrap(PyWindowObject *self, PyObject *args);
static PyObject *PyWindow_button_label(PyWindowObject *self, PyObject *args);
static PyObject *PyWindow_simple_chart(PyWindowObject *self, PyObject *args);
static PyObject *PyWindow_selectable_label(PyWindowObject *self, PyObject *args);
static PyObject *PyWindow_option_label(PyWindowObject *self, PyObject *args);
static PyObject *PyWindow_edit_string(PyWindowObject *self, PyObject *args);
static PyObject *PyWindow_group(PyWindowObject *self, PyObject *args);
static PyObject *PyWindow_tree(PyWindowObject *self, PyObject *args);
static PyObject *PyWindow_tree_element(PyWindowObject *self, PyObject *args);
static PyObject *PyWindow_selectable_symbol_label(PyWindowObject *self, PyObject *args);
static PyObject *PyWindow_combo_box(PyWindowObject *self, PyObject *args);
static PyObject *PyWindow_checkbox(PyWindowObject *self, PyObject *args);
static PyObject *PyWindow_color_picker(PyWindowObject *self, PyObject *args);
static PyObject *PyWindow_show(PyWindowObject *self);
static PyObject *PyWindow_hide(PyWindowObject *self);
static PyObject *PyWindow_update(PyWindowObject *self);
static PyObject *PyWindow_on_hide(PyWindowObject *self);
static PyObject *PyWindow_pickle(PyWindowObject *self);
static PyObject *PyWindow_unpickle(PyObject *cls, PyObject *args);
static PyObject *PyWindow_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
static void      PyWindow_dealloc(PyWindowObject *self);

static PyObject *PyWindow_get_pos(PyWindowObject *self, void *closure);
static PyObject *PyWindow_get_size(PyWindowObject *self, void *closure);
static PyObject *PyWindow_get_header_height(PyWindowObject *self, void *closure);
static PyObject *PyWindow_get_spacing(PyWindowObject *self, void *closure);
static int       PyWindow_set_spacing(PyWindowObject *self, PyObject *value, void *closure);
static PyObject *PyWindow_get_padding(PyWindowObject *self, void *closure);
static int       PyWindow_set_padding(PyWindowObject *self, PyObject *value, void *closure);
static PyObject *PyWindow_get_group_padding(PyWindowObject *self, void *closure);
static int       PyWindow_set_group_padding(PyWindowObject *self, PyObject *value, void *closure);
static PyObject *PyWindow_get_combo_padding(PyWindowObject *self, void *closure);
static int       PyWindow_set_combo_padding(PyWindowObject *self, PyObject *value, void *closure);
static PyObject *PyWindow_get_border(PyWindowObject *self, void *closure);
static int       PyWindow_set_border(PyWindowObject *self, PyObject *value, void *closure);
static PyObject *PyWindow_get_group_border(PyWindowObject *self, void *closure);
static int       PyWindow_set_group_border(PyWindowObject *self, PyObject *value, void *closure);
static PyObject *PyWindow_get_combo_border(PyWindowObject *self, void *closure);
static int       PyWindow_set_combo_border(PyWindowObject *self, PyObject *value, void *closure);
static PyObject *PyWindow_get_min_row_height_padding(PyWindowObject *self, void *closure);
static int       PyWindow_set_min_row_height_padding(PyWindowObject *self, PyObject *value, void *closure);
static PyObject *PyWindow_get_scrollbar_size(PyWindowObject *self, void *closure);
static int       PyWindow_set_scrollbar_size(PyWindowObject *self, PyObject *value, void *closure);
static PyObject *PyWindow_get_min_size(PyWindowObject *self, void *closure);
static int       PyWindow_set_min_size(PyWindowObject *self, PyObject *value, void *closure);
static PyObject *PyWindow_get_closed(PyWindowObject *self, void *closure);
static PyObject *PyWindow_get_hidden(PyWindowObject *self, void *closure);
static PyObject *PyWindow_get_interactive(PyWindowObject *self, void *closure);
static int       PyWindow_set_interactive(PyWindowObject *self, PyObject *value, void *closure);

/*****************************************************************************/
/* STATIC VARIABLES                                                          */
/*****************************************************************************/

static struct nk_context *s_nk_ctx;
static vec_win_t          s_active_windows;

static PyMethodDef PyWindow_methods[] = {
    {"layout_row_static", 
    (PyCFunction)PyWindow_layout_row_static, METH_VARARGS,
    "Add a row with a static layout."},

    {"layout_row_dynamic", 
    (PyCFunction)PyWindow_layout_row_dynamic, METH_VARARGS,
    "Add a row with a dynamic layout."},

    {"layout_row_begin", 
    (PyCFunction)PyWindow_layout_row_begin, METH_VARARGS,
    "Begin a new row to which widgets can be pushed."},

    {"layout_row_end", 
    (PyCFunction)PyWindow_layout_row_end, METH_NOARGS,
    "End a row previously started with 'layout_row_begin'."},

    {"layout_row_push", 
    (PyCFunction)PyWindow_layout_row_push, METH_VARARGS,
    "Add a widget to the currently active row. Note that this must be preceded by "
    "a call to 'layout_row_begin'."},

    {"label_colored", 
    (PyCFunction)PyWindow_label_colored, METH_VARARGS,
    "Add a colored label layout with the specified alignment."},

    {"label_colored_wrap", 
    (PyCFunction)PyWindow_label_colored_wrap, METH_VARARGS,
    "Add a colored label layout."},

    {"button_label", 
    (PyCFunction)PyWindow_button_label, METH_VARARGS,
    "Add a button with a label and action."},

    {"simple_chart", 
    (PyCFunction)PyWindow_simple_chart, METH_VARARGS,
    "Add a chart with a single slot."},

    {"selectable_label", 
    (PyCFunction)PyWindow_selectable_label, METH_VARARGS,
    "Adds a label that can be toggled to be selected with a mouse click. "
    "Returns the new state of the selectable label."},

    {"option_label", 
    (PyCFunction)PyWindow_option_label, METH_VARARGS,
    "Radio button with the specified text. Returns if the radio button is selected."},

    {"edit_string", 
    (PyCFunction)PyWindow_edit_string, METH_VARARGS,
    "Text field for getting string input from the user. Returns the current text."},

    {"group", 
    (PyCFunction)PyWindow_group, METH_VARARGS,
    "The window UI components pushed in the callable argument will be nested under a group."},

    {"tree", 
    (PyCFunction)PyWindow_tree, METH_VARARGS,
    "The window UI components pushed in the callable argument will be nested under a "
    "collapsable tree section."},

    {"tree_element", 
    (PyCFunction)PyWindow_tree_element, METH_VARARGS,
    "The window UI components pushed in the callable argument will be nested under a "
    "collapsable non-root tree section."},

    {"selectable_symbol_label", 
    (PyCFunction)PyWindow_selectable_symbol_label, METH_VARARGS,
    "Text label preceded by one of the pf.NK_SYMBOL_ symbols."},

    {"combo_box", 
    (PyCFunction)PyWindow_combo_box, METH_VARARGS,
    "Present a combo box with a list of selectable options."},

    {"checkbox", 
    (PyCFunction)PyWindow_checkbox, METH_VARARGS,
    "Checkbox which can be toggled. Returns True if checked."},

    {"color_picker", 
    (PyCFunction)PyWindow_color_picker, METH_VARARGS,
    "Graphical color picker widget. Returns the selected color as an RGBA tuple."},

    {"show", 
    (PyCFunction)PyWindow_show, METH_NOARGS,
    "Make the window visible."},

    {"hide", 
    (PyCFunction)PyWindow_hide, METH_NOARGS,
    "Make the window invisible."},

    {"update", 
    (PyCFunction)PyWindow_update, METH_NOARGS,
    "Handles layout and state changes of the window. Default implementation is empty. "
    "This method should be overridden by subclasses to customize the window look and behavior."},

    {"on_hide", 
    (PyCFunction)PyWindow_on_hide, METH_NOARGS,
    "Callback that gets invoked when the user hides the window with the close button."},

    {"__pickle__", 
    (PyCFunction)PyWindow_pickle, METH_NOARGS,
    "Serialize a Permafrost Engine window to a string."},

    {"__unpickle__", 
    (PyCFunction)PyWindow_unpickle, METH_VARARGS | METH_CLASS,
    "Create a new pf.Window instance from a string earlier returned from a __pickle__ method."
    "Returns a tuple of the new instance and the number of bytes consumed from the stream."},

    {NULL}  /* Sentinel */
};

static PyGetSetDef PyWindow_getset[] = {
    {"position",
    (getter)PyWindow_get_pos, NULL,
    "A tuple of two integers specifying the X and Y position of the window.",
    NULL},
    {"size",
    (getter)PyWindow_get_size, NULL,
    "A tuple of two integers specifying the width and height dimentions of the window.",
    NULL},
    {"header_height",
    (getter)PyWindow_get_header_height, NULL,
    "A float specifying the height of the window header in pixels.",
    NULL},
    {"spacing",
    (getter)PyWindow_get_spacing, 
    (setter)PyWindow_set_spacing,
    "An (X, Y) tuple of floats to control the spacing (between components) within a window.", 
    NULL},
    {"padding",
    (getter)PyWindow_get_padding, 
    (setter)PyWindow_set_padding,
    "An (X, Y) tuple of floats to control the padding (between border and content) of a window.", 
    NULL},
    {"group_padding",
    (getter)PyWindow_get_group_padding, 
    (setter)PyWindow_set_group_padding,
    "An (X, Y) tuple of floats to control the padding around a group in a window.", 
    NULL},
    {"combo_padding",
    (getter)PyWindow_get_combo_padding, 
    (setter)PyWindow_set_combo_padding,
    "An (X, Y) tuple of floats to control the padding around a combo section in a window.", 
    NULL},
    {"border",
    (getter)PyWindow_get_border, 
    (setter)PyWindow_set_border,
    "A float to control the border width of a window.", 
    NULL},
    {"group_border",
    (getter)PyWindow_get_group_border, 
    (setter)PyWindow_set_group_border,
    "A float to control the border width around a group.", 
    NULL},
    {"combo_border",
    (getter)PyWindow_get_group_border, 
    (setter)PyWindow_set_group_border,
    "A float to control the border width around a combo section.",
    NULL},
    {"min_row_height_padding",
    (getter)PyWindow_get_min_row_height_padding, 
    (setter)PyWindow_set_min_row_height_padding,
    "A float to control the minimum number of pixels of padding at the header and footer of a row..", 
    NULL},
    {"scrollbar_size",
    (getter)PyWindow_get_scrollbar_size, 
    (setter)PyWindow_set_scrollbar_size,
    "An (X, Y) tuple of floats to control the size of the scrollbar.", 
    NULL},
    {"min_size",
    (getter)PyWindow_get_min_size, 
    (setter)PyWindow_set_min_size,
    "An (X, Y) tuple of floats to control the minimum size of the window.", 
    NULL},
    {"closed",
    (getter)PyWindow_get_closed, NULL,
    "A readonly bool indicating if this window is 'closed'.",
    NULL},
    {"hidden",
    (getter)PyWindow_get_hidden, NULL,
    "A readonly bool indicating if this window is 'hidden'.",
    NULL},
    {"interactive",
    (getter)PyWindow_get_interactive, 
    (setter)PyWindow_set_interactive,
    "A read-write bool to enable or disable user interactivity for this window.",
    NULL},
    {NULL}  /* Sentinel */
};

static PyTypeObject PyWindow_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "pf.Window",               /* tp_name */
    sizeof(PyWindowObject),    /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)PyWindow_dealloc, /* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_reserved */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash  */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    "Permafrost Engine UI window.", /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    PyWindow_methods,          /* tp_methods */
    0,                         /* tp_members */
    PyWindow_getset,           /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)PyWindow_init,   /* tp_init */
    0,                         /* tp_alloc */
    PyWindow_new,              /* tp_new */
};

/*****************************************************************************/
/* STATIC FUNCTIONS                                                          */
/*****************************************************************************/

static bool equal(PyWindowObject *const *a, PyWindowObject *const *b)
{
    return *a == *b;
}

static int parse_float_pair(PyObject *tuple, float *out_a, float *out_b)
{
    if(!PyTuple_Check(tuple))
        return -1;

    PyObject *a = PyTuple_GetItem(tuple, 0);
    PyObject *b = PyTuple_GetItem(tuple, 1);

    if(!a || !b)
        return -1;

    if(!PyFloat_Check(a) || !PyFloat_Check(b))
        return -1;

    *out_a = PyFloat_AsDouble(a);
    *out_b = PyFloat_AsDouble(b);
    return 0;
}

static int PyWindow_init(PyWindowObject *self, PyObject *args, PyObject *kwargs)
{
    const char *name;
    struct rect rect;
    int flags;
    int vres[2];
    int resize_mask = ANCHOR_DEFAULT;
    static char *kwlist[] = { "name", "bounds", "flags", "virtual_resolution", "resize_mask", NULL };

    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "s(iiii)i(ii)|i", kwlist, &name, &rect.x, &rect.y, 
                                    &rect.w, &rect.h, &flags, &vres[0], &vres[1], &resize_mask)) {
        PyErr_SetString(PyExc_TypeError, "4 arguments expected: integer, tuple of 4 integers, integer, and a tuple of 2 integers.");
        return -1;
    }

    if((resize_mask & ANCHOR_X_MASK) == 0
    || (resize_mask & ANCHOR_Y_MASK) == 0) {

        PyErr_SetString(PyExc_RuntimeError, "Invalid reisize mask: the window must have at least one anchor in each dimension.");
        return -1;
    }

    pf_strlcpy(self->name, name, sizeof(self->name));
    self->rect = rect;
    self->flags = flags;
    self->style = s_nk_ctx->style.window;
    self->resize_mask = resize_mask;
    self->virt_res.x = vres[0];
    self->virt_res.y = vres[1];

    self->flags |= (NK_WINDOW_CLOSED | NK_WINDOW_HIDDEN); /* closed by default */
    return 0;
}

static PyObject *PyWindow_layout_row_static(PyWindowObject *self, PyObject *args)
{
    int height, width, cols;

    if(!PyArg_ParseTuple(args, "iii", &height, &width, &cols)) {
        PyErr_SetString(PyExc_TypeError, "Arguments must be three integers.");
        return NULL;
    }
    nk_layout_row_static(s_nk_ctx, height, width, cols);
    Py_RETURN_NONE;
}

static PyObject *PyWindow_layout_row_dynamic(PyWindowObject *self, PyObject *args)
{
    int height, cols;

    if(!PyArg_ParseTuple(args, "ii", &height, &cols)) {
        PyErr_SetString(PyExc_TypeError, "Arguments must be three integers.");
        return NULL;
    }
    nk_layout_row_dynamic(s_nk_ctx, height, cols);
    Py_RETURN_NONE;
}

static PyObject *PyWindow_layout_row_begin(PyWindowObject *self, PyObject *args)
{
    int layout_fmt;
    int height, cols;

    if(!PyArg_ParseTuple(args, "iii", &layout_fmt, &height, &cols)) {
        PyErr_SetString(PyExc_TypeError, "Arguments must be three integers.");
        return NULL;
    }

    if(layout_fmt != NK_STATIC && layout_fmt != NK_DYNAMIC) {
        PyErr_SetString(PyExc_TypeError, "First argument must be 0 or 1.");
        return NULL;
    }

    nk_layout_row_begin(s_nk_ctx, layout_fmt, height, cols);
    Py_RETURN_NONE;
}

static PyObject *PyWindow_layout_row_end(PyWindowObject *self)
{
    nk_layout_row_end(s_nk_ctx);
    Py_RETURN_NONE;
}

static PyObject *PyWindow_layout_row_push(PyWindowObject *self, PyObject *args)
{
    int width;

    if(!PyArg_ParseTuple(args, "i", &width)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a single integer.");
        return NULL;
    }

    nk_layout_row_push(s_nk_ctx, width);
    Py_RETURN_NONE; 
}

static PyObject *PyWindow_label_colored(PyWindowObject *self, PyObject *args)
{
    const char *text;
    int alignment;
    int r, g, b;

    if(!PyArg_ParseTuple(args, "si(iii)", &text, &alignment, &r, &g, &b)) {
        PyErr_SetString(PyExc_TypeError, "3 arguments expected: a string, an integer and a tuple of 3 integers.");
        return NULL;
    }

    nk_label_colored(s_nk_ctx, text, alignment, nk_rgb(r, g, b));
    Py_RETURN_NONE;  
}

static PyObject *PyWindow_label_colored_wrap(PyWindowObject *self, PyObject *args)
{
    const char *text;
    int r, g, b;

    if(!PyArg_ParseTuple(args, "s(iii)", &text, &r, &g, &b)) {
        PyErr_SetString(PyExc_TypeError, "2 arguments expected: a string and a tuple of 3 integers.");
        return NULL;
    }

    nk_label_colored_wrap(s_nk_ctx, text, nk_rgb(r, g, b));
    Py_RETURN_NONE;  
}

static PyObject *PyWindow_button_label(PyWindowObject *self, PyObject *args)
{
    const char *str;
    PyObject *callable;

    if(!PyArg_ParseTuple(args, "sO", &str, &callable)) {
        PyErr_SetString(PyExc_TypeError, "Arguments must be a string and an object.");
        return NULL;
    }

    if(!PyCallable_Check(callable)) {
        PyErr_SetString(PyExc_TypeError, "Second argument must be callable.");
        return NULL;
    }

    if(nk_button_label(s_nk_ctx, str)) {
        PyObject *ret = PyObject_CallObject(callable, NULL);
        Py_XDECREF(ret);
    }

    Py_RETURN_NONE;
}

static PyObject *PyWindow_simple_chart(PyWindowObject *self, PyObject *args)
{
    int type;
    int min, max;
    PyObject *list, *on_click_handler = NULL;

    int clicked_index = -1;
    int hovered_index = -1;
    long hovered_val;

    if(!PyArg_ParseTuple(args, "i(ii)O|O", &type, &min, &max, &list, &on_click_handler)) {
        PyErr_SetString(PyExc_TypeError, "3 arguments expected: an integer, a tuple of two integers, and a list object. "
            "Optionally, a callable taking exactly one integer index argument (click handler) can additionally be supplied.");
        return NULL;
    }

    if(!PyList_Check(list)) {
        PyErr_SetString(PyExc_TypeError, "Third argument must be a list.");
        return NULL;
    }

    unsigned num_datapoints = PyList_Size(list);
    if(nk_chart_begin(s_nk_ctx, type, num_datapoints, min, max)) {
    
        for(int i = 0; i < num_datapoints; i++) {
        
            PyObject *elem = PyList_GetItem(list, i);
            if(!PyInt_Check(elem)) {
                PyErr_SetString(PyExc_TypeError, "List elements must be integers.");
                return NULL;
            }

            long val = PyInt_AsLong(elem);
            nk_flags res = nk_chart_push(s_nk_ctx, val);

            if(res & NK_CHART_HOVERING) {
                hovered_index = i;
                hovered_val = val;
            }

            if((res & NK_CHART_CLICKED) && on_click_handler) {
                clicked_index = i;
            }
        }
        nk_chart_end(s_nk_ctx);

        if(hovered_index != -1)
            nk_tooltipf(s_nk_ctx, "Value: %lu", hovered_val);

        if(clicked_index != -1 && on_click_handler) {
            PyObject *args = Py_BuildValue("(i)", clicked_index);
            if(args) {
                PyObject *ret = PyObject_CallObject(on_click_handler, args);
                Py_DECREF(args);
                Py_XDECREF(ret);
            }
        }
    }

    Py_RETURN_NONE;
}

static PyObject *PyWindow_selectable_label(PyWindowObject *self, PyObject *args)
{
    const char *text; 
    int align_flags;
    int on;

    if(!PyArg_ParseTuple(args, "sii", &text, &align_flags, &on)) {
        PyErr_SetString(PyExc_TypeError, "Arguments must be a string and two integers.");
        return NULL;
    }

    nk_selectable_label(s_nk_ctx, text, align_flags, &on);
    if(0 == on)
        Py_RETURN_FALSE;
    else
        Py_RETURN_TRUE;
}

static PyObject *PyWindow_option_label(PyWindowObject *self, PyObject *args)
{
    const char *text; 
    int set;

    if(!PyArg_ParseTuple(args, "si", &text, &set)) {
        PyErr_SetString(PyExc_TypeError, "Arguments must be a string and an integer.");
        return NULL;
    }

    set = nk_option_label(s_nk_ctx, text, set);
    if(0 == set)
        Py_RETURN_FALSE;
    else
        Py_RETURN_TRUE;
}

static PyObject *PyWindow_edit_string(PyWindowObject *self, PyObject *args)
{
    int flags;
    const char *str;

    if(!PyArg_ParseTuple(args, "is", &flags, &str)) {
        PyErr_SetString(PyExc_TypeError, "Arguments must be an integer and a string.");
        return NULL;
    }

    char textbuff[128];
    int len = strlen(str);

    assert(len < sizeof(textbuff));
    strcpy(textbuff, str);

    nk_edit_string(s_nk_ctx, flags, textbuff, &len, sizeof(textbuff), nk_filter_default);
    textbuff[len] = '\0';
    return Py_BuildValue("s", textbuff);
}

static PyObject *PyWindow_group(PyWindowObject *self, PyObject *args)
{
    const char *name;
    int group_flags;
    PyObject *callable, *cargs = NULL;

    if(!PyArg_ParseTuple(args, "siO|O", &name, &group_flags, &callable)) {
        PyErr_SetString(PyExc_TypeError, "Arguments must be a string, an integer and an object. "
            "Optionally, args to the callable can be supplied.");
        return NULL;
    }

    if(!PyCallable_Check(callable)) {
        PyErr_SetString(PyExc_TypeError, "Second argument must be callable.");
        return NULL;
    }

    if(nk_group_begin(s_nk_ctx, name, group_flags)) {
        PyObject *ret = PyObject_CallObject(callable, NULL);
        Py_XDECREF(ret);
    }
    nk_group_end(s_nk_ctx);
    Py_RETURN_NONE;
}

static PyObject *PyWindow_tree(PyWindowObject *self, PyObject *args)
{
    int type, state;
    const char *name;
    PyObject *callable, *cargs = NULL;

    if(!PyArg_ParseTuple(args, "isiO|O", &type, &name, &state, &callable, &cargs)) {
        PyErr_SetString(PyExc_TypeError, "Invalid arguments. Expecting: (type, name, state, callable, [args])");
        return NULL;
    }

    if(type != NK_TREE_TAB && type != NK_TREE_NODE) {
        PyErr_SetString(PyExc_TypeError, "First argument must be one of pf.NK_TREE_TAB or pf.NK_TREE_NODE.");
        return NULL;
    }

    if(state != NK_MINIMIZED && state != NK_MAXIMIZED) {
        PyErr_SetString(PyExc_TypeError, "Third argument must be one of pf.NK_MINIMIZED or pf.NK_MAXIMIZED.");
        return NULL;
    }

    if(!PyCallable_Check(callable)) {
        PyErr_SetString(PyExc_TypeError, "Fourth argument must be callable.");
        return NULL;
    }

    if(cargs && !PyTuple_Check(cargs)) {
        PyErr_SetString(PyExc_TypeError, "(Optional) fifth argument must be a tuple.");
        return NULL;
    }

    bool shown;
    if((shown = nk_tree_push_hashed(s_nk_ctx, type, name, state, name, strlen(name), (uintptr_t)self))) {
        PyObject *ret = PyObject_CallObject(callable, cargs);
        Py_XDECREF(ret);
        nk_tree_pop(s_nk_ctx);
    }

    if(shown)
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static PyObject *PyWindow_tree_element(PyWindowObject *self, PyObject *args)
{
    int type, state;
    const char *name;
    PyObject *callable, *selected, *cargs = NULL;

    if(!PyArg_ParseTuple(args, "isiOO|O", &type, &name, &state, &selected, &callable, &cargs)) {
        PyErr_SetString(PyExc_TypeError, "Invalida arguments. Expecting: (type, name, state, selected, callable, [args])");
        return NULL;
    }

    if(type != NK_TREE_TAB && type != NK_TREE_NODE) {
        PyErr_SetString(PyExc_TypeError, "First argument must be one of pf.NK_TREE_TAB or pf.NK_TREE_NODE.");
        return NULL;
    }

    if(state != NK_MINIMIZED && state != NK_MAXIMIZED) {
        PyErr_SetString(PyExc_TypeError, "Third argument must be one of pf.NK_MINIMIZED or pf.NK_MAXIMIZED.");
        return NULL;
    }

    if(!PyCallable_Check(callable)) {
        PyErr_SetString(PyExc_TypeError, "Fifth argument must be callable.");
        return NULL;
    }

    if(cargs && !PyTuple_Check(cargs)) {
        PyErr_SetString(PyExc_TypeError, "(Optional) sixth argument must be a tuple.");
        return NULL;
    }

    int sel = PyObject_IsTrue(selected);
    if(nk_tree_element_push_hashed(s_nk_ctx, type, name, state, &sel, name, strlen(name), (uintptr_t)self)) {
        PyObject *ret = PyObject_CallObject(callable, cargs);
        Py_XDECREF(ret);
        nk_tree_pop(s_nk_ctx);
    }

    if(sel)
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static PyObject *PyWindow_selectable_symbol_label(PyWindowObject *self, PyObject *args)
{
    int symbol, align;
    const char *title;
    PyObject *selected;

    if(!PyArg_ParseTuple(args, "isiO", &symbol, &title, &align, &selected)) {
        PyErr_SetString(PyExc_TypeError, "Invalida arguments. Expecting: (symbol, title, alignment, selected)");
        return NULL;
    }

    if(symbol < 0 || symbol >= NK_SYMBOL_MAX) {
        PyErr_SetString(PyExc_TypeError, "First argument must be one of the pf.NK_SYMBOL_ constants.");
        return NULL;
    }

    if(align != NK_TEXT_LEFT && align != NK_TEXT_RIGHT && align != NK_TEXT_CENTERED) {
        PyErr_SetString(PyExc_TypeError, "Third argument must be one of: pf.NK_TEXT_LEFT, pf.NK_TEXT_CENTERED, pf.NK_TEXT_RIGHT.");
        return NULL;
    }

    int sel = PyObject_IsTrue(selected);
    nk_selectable_symbol_label(s_nk_ctx, symbol, title, align, &sel);

    if(sel)
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static PyObject *PyWindow_combo_box(PyWindowObject *self, PyObject *args)
{
    PyObject *items_list;
    int selected_idx;
    int item_height;
    struct nk_vec2 size;

    if(!PyArg_ParseTuple(args, "Oii(ff)", &items_list, &selected_idx, &item_height, &size.x, &size.y)) {
        PyErr_SetString(PyExc_TypeError, "Arguments must be an object, two integers, and a tuple of two floats.");
        return NULL;
    }

    if(!PyList_Check(items_list)) {
        PyErr_SetString(PyExc_TypeError, "First argument must be a list.");
        return NULL;
    }

    size_t num_items = PyList_Size(items_list);
    const char *labels[num_items];

    for(int i = 0; i < num_items; i++) {

        PyObject *str = PyList_GetItem(items_list, i);
        if(!PyString_Check(str)) {
            PyErr_SetString(PyExc_TypeError, "First argument list must only contain strings.");
            return NULL;
        }
        labels[i] = PyString_AsString(str);
    }

    int ret = nk_combo(s_nk_ctx, labels, num_items, selected_idx, item_height, size);
    return Py_BuildValue("i", ret);
}

static PyObject *PyWindow_checkbox(PyWindowObject *self, PyObject *args)
{
    const char *label;
    int selected;

    if(!PyArg_ParseTuple(args, "si", &label, &selected)) {
        PyErr_SetString(PyExc_TypeError, "Arguments must be a string and an integer.");
        return NULL;
    }

    nk_checkbox_label(s_nk_ctx, label, &selected);
    return Py_BuildValue("i", selected);
}

static PyObject *PyWindow_color_picker(PyWindowObject *self, PyObject *args)
{
    struct nk_color color;
    struct nk_colorf colorf;
    struct nk_vec2 size;

    if(!PyArg_ParseTuple(args, "(iiii)(ff)", &color.r, &color.g, &color.b, &color.a, &size.x, &size.y)) {
        PyErr_SetString(PyExc_TypeError, "Arguments must be a tuple of 4 ints and a tuple of 2 floats.");
        return NULL;
    }

    if(nk_combo_begin_color(s_nk_ctx, color, nk_vec2(size.x, size.y+10))) {
    
        nk_layout_row_dynamic(s_nk_ctx, size.y, 1);
        colorf = nk_color_picker(s_nk_ctx, nk_color_cf(color), NK_RGB);
        color = nk_rgba_cf(colorf);
        nk_combo_end(s_nk_ctx);
    }

    return Py_BuildValue("(i,i,i,i)", color.r, color.g, color.b, color.a);
}

static PyObject *PyWindow_show(PyWindowObject *self)
{
    self->flags &= ~(NK_WINDOW_HIDDEN | NK_WINDOW_CLOSED);
    nk_window_show(s_nk_ctx, self->name, NK_SHOWN);
    Py_RETURN_NONE;
}

static PyObject *PyWindow_hide(PyWindowObject *self)
{
    self->flags |= (NK_WINDOW_HIDDEN | NK_WINDOW_CLOSED);
    Py_RETURN_NONE;
}

static PyObject *PyWindow_update(PyWindowObject *self)
{
    Py_RETURN_NONE;
}

static PyObject *PyWindow_on_hide(PyWindowObject *self)
{
    Py_RETURN_NONE;
}

static PyObject *PyWindow_pickle(PyWindowObject *self)
{
    bool status;
    PyObject *ret = NULL;

    SDL_RWops *stream = PFSDL_VectorRWOps();
    CHK_TRUE(stream, fail_alloc);

    PyObject *name = PyString_FromString(self->name);
    CHK_TRUE(name, fail_pickle);
    status = S_PickleObjgraph(name, stream);
    Py_DECREF(name);
    CHK_TRUE(status, fail_pickle);

    PyObject *rect = Py_BuildValue("(iiii)", 
        self->rect.x, self->rect.y, self->rect.w, self->rect.h);
    CHK_TRUE(rect, fail_pickle);
    status = S_PickleObjgraph(rect, stream);
    Py_DECREF(rect);
    CHK_TRUE(status, fail_pickle);

    PyObject *flags = PyInt_FromLong(self->flags);
    CHK_TRUE(flags, fail_pickle);
    status = S_PickleObjgraph(flags, stream);
    Py_DECREF(flags);
    CHK_TRUE(status, fail_pickle);

    PyObject *virt_res = Py_BuildValue("(ii)", self->virt_res.x, self->virt_res.y);
    CHK_TRUE(virt_res, fail_pickle);
    status = S_PickleObjgraph(virt_res, stream);
    Py_DECREF(virt_res);
    CHK_TRUE(status, fail_pickle);

    PyObject *resize_mask = PyInt_FromLong(self->resize_mask);
    CHK_TRUE(resize_mask, fail_pickle);
    status = S_PickleObjgraph(resize_mask, stream);
    Py_DECREF(resize_mask);
    CHK_TRUE(status, fail_pickle);

    CHK_TRUE_RET(S_UI_Style_SaveWindow(stream, &self->style));
    ret = PyString_FromStringAndSize(PFSDL_VectorRWOpsRaw(stream), SDL_RWsize(stream));

fail_pickle:
    SDL_RWclose(stream);
fail_alloc:
    return ret;
}

static PyObject *PyWindow_unpickle(PyObject *cls, PyObject *args)
{
    PyObject *ret = NULL;
    const char *str;
    Py_ssize_t len;
    int status;
    char tmp;

    if(!PyArg_ParseTuple(args, "s#", &str, &len)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a single string.");
        goto fail_args;
    }

    SDL_RWops *stream = SDL_RWFromConstMem(str, len);
    CHK_TRUE(stream, fail_args);

    PyObject *name = S_UnpickleObjgraph(stream);
    SDL_RWread(stream, &tmp, 1, 1); /* consume NULL byte */

    PyObject *rect = S_UnpickleObjgraph(stream);
    SDL_RWread(stream, &tmp, 1, 1); /* consume NULL byte */

    PyObject *flags = S_UnpickleObjgraph(stream);
    SDL_RWread(stream, &tmp, 1, 1); /* consume NULL byte */

    PyObject *virt_res = S_UnpickleObjgraph(stream);
    SDL_RWread(stream, &tmp, 1, 1); /* consume NULL byte */

    PyObject *resize_mask = S_UnpickleObjgraph(stream);
    SDL_RWread(stream, &tmp, 1, 1); /* consume NULL byte */

    if(!name
    || !rect
    || !flags
    || !resize_mask
    || !virt_res) {
        PyErr_SetString(PyExc_RuntimeError, "Could not unpickle internal state of pf.Window instance");
        goto fail_unpickle;
    }

    PyObject *win_args = Py_BuildValue("(OOOOO)", name, rect, flags, virt_res, resize_mask);
    PyWindowObject *winobj = (PyWindowObject*)((PyTypeObject*)cls)->tp_new((struct _typeobject*)cls, win_args, NULL);
    assert(winobj || PyErr_Occurred());
    CHK_TRUE(winobj, fail_window);

    const char *namestr;
    if(!PyArg_ParseTuple(win_args, "s(iiii)i(ii)|i", &namestr,
        &winobj->rect.x, &winobj->rect.y, 
        &winobj->rect.w, &winobj->rect.h, 
        &winobj->flags, 
        &winobj->virt_res.x, &winobj->virt_res.y, 
        &winobj->resize_mask)) {
        goto fail_window;
    }
    pf_strlcpy(winobj->name, namestr, sizeof(winobj->name));

    if(!S_UI_Style_LoadWindow(stream, &winobj->style)) {
        PyErr_SetString(PyExc_RuntimeError, "Could not unpickle style state of pf.Window instance");
        goto fail_window;
    }

    Py_ssize_t nread = SDL_RWseek(stream, 0, RW_SEEK_CUR);
    ret = Py_BuildValue("(Oi)", winobj, (int)nread);

fail_window:
    Py_XDECREF(winobj);
    Py_XDECREF(win_args);
fail_unpickle:
    Py_XDECREF(name);
    Py_XDECREF(rect);
    Py_XDECREF(flags);
    Py_XDECREF(resize_mask);
    Py_XDECREF(virt_res);
    SDL_RWclose(stream);
fail_args:
    return ret;
}

static PyObject *PyWindow_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyObject *self = type->tp_alloc(type, 0);
    vec_win_push(&s_active_windows, (PyWindowObject*)self);
    return self;
}

static void PyWindow_dealloc(PyWindowObject *self)
{
    int idx;
    vec_win_indexof(&s_active_windows, self, equal, &idx);
    vec_win_del(&s_active_windows, idx);

    nk_window_close(s_nk_ctx, self->name);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject *PyWindow_get_pos(PyWindowObject *self, void *closure)
{
    return Py_BuildValue("(ii)", self->rect.x, self->rect.y);
}

static PyObject *PyWindow_get_size(PyWindowObject *self, void *closure)
{
    return Py_BuildValue("(ii)", self->rect.w, self->rect.h);
}

static PyObject *PyWindow_get_header_height(PyWindowObject *self, void *closure)
{
    float header_height = s_nk_ctx->style.font->height
                        + 2.0f * self->style.header.padding.y
                        + 2.0f * self->style.header.label_padding.y;
    return Py_BuildValue("i", (int)header_height);
}

static PyObject *PyWindow_get_spacing(PyWindowObject *self, void *closure)
{
    return Py_BuildValue("(f, f)",
        self->style.spacing.x,
        self->style.spacing.y); 
}

static int PyWindow_set_spacing(PyWindowObject *self, PyObject *value, void *closure)
{
    float x, y;

    if(parse_float_pair(value, &x, &y) != 0) {
        PyErr_SetString(PyExc_TypeError, "Type must be a tuple of 2 floats.");
        return -1; 
    }

    self->style.spacing = (struct nk_vec2){x, y};
    return 0;
}

static PyObject *PyWindow_get_padding(PyWindowObject *self, void *closure)
{
    return Py_BuildValue("(f, f)",
        self->style.padding.x,
        self->style.padding.y);
}

static int PyWindow_set_padding(PyWindowObject *self, PyObject *value, void *closure)
{
    float x, y;

    if(parse_float_pair(value, &x, &y) != 0) {
        PyErr_SetString(PyExc_TypeError, "Type must be a tuple of 2 floats.");
        return -1; 
    }

    self->style.padding = nk_vec2(x,y);
    return 0;
}

static PyObject *PyWindow_get_group_padding(PyWindowObject *self, void *closure)
{
    return Py_BuildValue("(f, f)",
        self->style.group_padding.x,
        self->style.group_padding.y);
}

static int PyWindow_set_group_padding(PyWindowObject *self, PyObject *value, void *closure)
{
    float x, y;

    if(parse_float_pair(value, &x, &y) != 0) {
        PyErr_SetString(PyExc_TypeError, "Type must be a tuple of 2 floats.");
        return -1; 
    }

    self->style.group_padding = nk_vec2(x,y);
    return 0;
}

static PyObject *PyWindow_get_combo_padding(PyWindowObject *self, void *closure)
{
    return Py_BuildValue("(f, f)",
        self->style.combo_padding.x,
        self->style.combo_padding.y);
}

static int PyWindow_set_combo_padding(PyWindowObject *self, PyObject *value, void *closure)
{
    float x, y;

    if(parse_float_pair(value, &x, &y) != 0) {
        PyErr_SetString(PyExc_TypeError, "Type must be a tuple of 2 floats.");
        return -1; 
    }

    self->style.combo_padding = nk_vec2(x,y);
    return 0;
}

static PyObject *PyWindow_get_border(PyWindowObject *self, void *closure)
{
    return Py_BuildValue("f", self->style.border);
}

static int PyWindow_set_border(PyWindowObject *self, PyObject *value, void *closure)
{
    float border;
    if(!PyArg_ParseTuple(value, "f", &border)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a float.");
        return -1;
    }

    self->style.border = border;
    return 0;
}

static PyObject *PyWindow_get_group_border(PyWindowObject *self, void *closure)
{
    return Py_BuildValue("f", self->style.group_border);
}

static int PyWindow_set_group_border(PyWindowObject *self, PyObject *value, void *closure)
{
    float border;
    if(!PyArg_ParseTuple(value, "f", &border)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a float.");
        return -1;
    }

    self->style.group_border = border;
    return 0;
}

static PyObject *PyWindow_get_combo_border(PyWindowObject *self, void *closure)
{
    return Py_BuildValue("f", self->style.combo_border);
}

static int PyWindow_set_combo_border(PyWindowObject *self, PyObject *value, void *closure)
{
    float border;
    if(!PyArg_ParseTuple(value, "f", &border)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a float.");
        return -1;
    }

    self->style.combo_border = border;
    return 0;
}

static PyObject *PyWindow_get_min_row_height_padding(PyWindowObject *self, void *closure)
{
    return Py_BuildValue("f", self->style.min_row_height_padding);
}

static int PyWindow_set_min_row_height_padding(PyWindowObject *self, PyObject *value, void *closure)
{
    float min;
    if(!PyArg_ParseTuple(value, "f", &min)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a float.");
        return -1;
    }

    self->style.min_row_height_padding = min;
    return 0;
}

static PyObject *PyWindow_get_scrollbar_size(PyWindowObject *self, void *closure)
{
    return Py_BuildValue("(f, f)", 
        self->style.scrollbar_size.x, 
        self->style.scrollbar_size.y);
}

static int PyWindow_set_scrollbar_size(PyWindowObject *self, PyObject *value, void *closure)
{
    float x, y;

    if(parse_float_pair(value, &x, &y) != 0) {
        PyErr_SetString(PyExc_TypeError, "Type must be a tuple of 2 floats.");
        return -1; 
    }

    self->style.scrollbar_size = nk_vec2(x,y);
    return 0;
}

static PyObject *PyWindow_get_min_size(PyWindowObject *self, void *closure)
{
    return Py_BuildValue("(f, f)", 
        self->style.min_size.x, 
        self->style.min_size.y);
}

static int PyWindow_set_min_size(PyWindowObject *self, PyObject *value, void *closure)
{
    float x, y;

    if(parse_float_pair(value, &x, &y) != 0) {
        PyErr_SetString(PyExc_TypeError, "Type must be a tuple of 2 floats.");
        return -1; 
    }

    self->style.min_size = nk_vec2(x,y);
    return 0;
}

static PyObject *PyWindow_get_closed(PyWindowObject *self, void *closure)
{
    if(self->flags & NK_WINDOW_CLOSED) 
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static PyObject *PyWindow_get_hidden(PyWindowObject *self, void *closure)
{
    if(self->flags & NK_WINDOW_HIDDEN) 
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static PyObject *PyWindow_get_interactive(PyWindowObject *self, void *closure)
{
    if(self->flags & NK_WINDOW_NOT_INTERACTIVE)
        Py_RETURN_FALSE;
    else
        Py_RETURN_TRUE;
}

static int PyWindow_set_interactive(PyWindowObject *self, PyObject *value, void *closure)
{
    if(PyObject_IsTrue(value))
        self->flags &= ~NK_WINDOW_NOT_INTERACTIVE;
    else
        self->flags |= NK_WINDOW_NOT_INTERACTIVE;
    return 0;
}

static void call_critfail(PyObject *obj, char *method_name)
{
    PyObject *ret = PyObject_CallMethod(obj, method_name, NULL);
    Py_XDECREF(ret);
    if(!ret) {
        PyErr_Print();
        exit(EXIT_FAILURE);
    }
}

static void active_windows_update(void *user, void *event)
{
    (void)user;
    (void)event;

    for(int i = 0; i < vec_size(&s_active_windows); i++) {
    
        PyWindowObject *win = vec_AT(&s_active_windows, i);
        if(win->flags & (NK_WINDOW_HIDDEN | NK_WINDOW_CLOSED))
            continue;

        struct nk_style_window saved_style = s_nk_ctx->style.window;
        s_nk_ctx->style.window = win->style;

        struct nk_vec2i adj_vres = TO_VEC2I(UI_ArAdjustedVRes(TO_VEC2T(win->virt_res)));
        struct rect adj_bounds = UI_BoundsForAspectRatio(win->rect, 
            TO_VEC2T(win->virt_res), TO_VEC2T(adj_vres), win->resize_mask);

        if(nk_begin_with_vres(s_nk_ctx, win->name, 
            nk_rect(adj_bounds.x, adj_bounds.y, adj_bounds.w, adj_bounds.h), win->flags, adj_vres)) {

            call_critfail((PyObject*)win, "update");
        }

        if(s_nk_ctx->current->flags & NK_WINDOW_HIDDEN && !(win->flags & NK_WINDOW_HIDDEN)) {
        
            call_critfail((PyObject*)win, "on_hide");
        }

        struct nk_vec2 pos = nk_window_get_position(s_nk_ctx);
        struct nk_vec2 size = nk_window_get_size(s_nk_ctx);
        adj_bounds = (struct rect){pos.x, pos.y, size.x, size.y};
        win->rect = UI_BoundsForAspectRatio(adj_bounds, TO_VEC2T(adj_vres), TO_VEC2T(win->virt_res), win->resize_mask);

        int sample_mask = NK_WINDOW_HIDDEN | NK_WINDOW_CLOSED;
        win->flags &= ~sample_mask; 
        win->flags |= (s_nk_ctx->current->flags & sample_mask);

        nk_end(s_nk_ctx);
        s_nk_ctx->style.window = saved_style;
    }
}

/*****************************************************************************/
/* EXTERN FUNCTIONS                                                          */
/*****************************************************************************/

bool S_UI_Init(struct nk_context *ctx)
{
    assert(ctx);
    s_nk_ctx = ctx;
    vec_win_init(&s_active_windows);

    if(!E_Global_Register(EVENT_UPDATE_UI, active_windows_update, NULL, G_RUNNING | G_PAUSED_UI_RUNNING))
        return false;
    return true;
}

void S_UI_Shutdown(void)
{
    E_Global_Unregister(EVENT_UPDATE_UI, active_windows_update);
    vec_win_destroy(&s_active_windows);
}

void S_UI_PyRegister(PyObject *module)
{
    if(PyType_Ready(&PyWindow_type) < 0)
        return;
    Py_INCREF(&PyWindow_type);
    PyModule_AddObject(module, "Window", (PyObject*)&PyWindow_type);

    assert(s_nk_ctx);
    S_UI_Style_PyRegister(module, s_nk_ctx);
}

bool S_UI_MouseOverWindow(int mouse_x, int mouse_y)
{
    int w, h;    
    Engine_WinDrawableSize(&w, &h);

    for(int i = 0; i < vec_size(&s_active_windows); i++) {

        PyWindowObject *win = vec_AT(&s_active_windows, i);
        struct rect adj_bounds = UI_BoundsForAspectRatio(win->rect, TO_VEC2T(win->virt_res),
            UI_ArAdjustedVRes(TO_VEC2T(win->virt_res)), win->resize_mask);
        struct nk_vec2 visible_size = {adj_bounds.w, adj_bounds.h};

        if(win->flags & NK_WINDOW_HIDDEN
        || win->flags & NK_WINDOW_CLOSED) {
            continue; 
        }

        int vmouse_x = (float)mouse_x / w * UI_ArAdjustedVRes(TO_VEC2T(win->virt_res)).x;
        int vmouse_y = (float)mouse_y / h * UI_ArAdjustedVRes(TO_VEC2T(win->virt_res)).y;

        /* For minimized windows, only the header is visible */
        struct nk_window *nkwin = nk_window_find(s_nk_ctx, win->name);
        if(nkwin && nkwin->flags & NK_WINDOW_MINIMIZED) {
        
            float header_height = s_nk_ctx->style.font->height
                                + 2.0f * s_nk_ctx->style.window.header.padding.y
                                + 2.0f * s_nk_ctx->style.window.header.label_padding.y;
            visible_size.y = header_height;
        }

        if(C_PointInsideRect2D(
            (vec2_t){vmouse_x,                      vmouse_y},
            (vec2_t){adj_bounds.x,                  adj_bounds.y},
            (vec2_t){adj_bounds.x + visible_size.x, adj_bounds.y},
            (vec2_t){adj_bounds.x + visible_size.x, adj_bounds.y + visible_size.y},
            (vec2_t){adj_bounds.x,                  adj_bounds.y + visible_size.y}))
            return true;
    }

    return false;
}

bool S_UI_TextEditHasFocus(void)
{
    for(int i = 0; i < vec_size(&s_active_windows); i++) {

        PyWindowObject *win = vec_AT(&s_active_windows, i);

        if(win->flags & NK_WINDOW_HIDDEN
        || win->flags & NK_WINDOW_CLOSED) {
            continue; 
        }

        struct nk_window *nkwin = nk_window_find(s_nk_ctx, win->name);
        assert(nkwin);
        if(nkwin->edit.active == nk_true)
            return true;
    }

    return false;
}

