#include <Python.h>
#include <libpostal/libpostal.h>

#if PY_MAJOR_VERSION >= 3
#define IS_PY3K
#endif

struct module_state {
    PyObject *error;
};


#ifdef IS_PY3K
    #define GETSTATE(m) ((struct module_state*)PyModule_GetState(m))
#else
    #define GETSTATE(m) (&_state)
    static struct module_state _state;
#endif


static PyObject *py_parse_address(PyObject *self, PyObject *args, PyObject *keywords) {
    PyObject *arg_input;
    PyObject *arg_language = Py_None;
    PyObject *arg_country = Py_None;

    PyObject *result = NULL;

    static char *kwlist[] = {"address",
                             "language",
                             "country",
                             NULL
                            };


    if (!PyArg_ParseTupleAndKeywords(args, keywords, 
                                     "O|OO:pyparser", kwlist,
                                     &arg_input, &arg_language,
                                     &arg_country
                                     )) {
        return 0;
    }

    PyObject *unistr_input = PyUnicode_FromObject(arg_input);
    if (unistr_input == NULL) {
        PyErr_SetString(PyExc_TypeError,
                        "Input could not be converted to unicode");
        return 0;
    }

    char *input = NULL;

    #ifdef IS_PY3K
        // Python 3 encoding, supported by Python 3.3+

        input = PyUnicode_AsUTF8(unistr_input);

    #else
        // Python 2 encoding

        PyObject *str_input = PyUnicode_AsEncodedString(unistr_input, "utf-8", "strict");
        if (str_input == NULL) {
            PyErr_SetString(PyExc_TypeError,
                            "Input could not be utf-8 encoded");
            goto exit_decref_input_unistr;
        }

        input = PyBytes_AsString(str_input);
    #endif

    if (input == NULL) {
        goto exit_decref_input_str;
    }

    char *language = NULL;

    PyObject *unistr_language = Py_None;
    PyObject *str_language = Py_None;

    if (arg_language != Py_None) {
        unistr_language = PyUnicode_FromObject(arg_language);
        if (unistr_language == NULL) {
            PyErr_SetString(PyExc_TypeError,
                            "Language could not be converted to unicode");
        }

        #ifdef IS_PY3K
            // Python 3 encoding, supported by Python 3.3+

            language = PyUnicode_AsUTF8(unistr_language);

        #else
            // Python 2 encoding

            PyObject *str_language = PyUnicode_AsEncodedString(unistr_language, "utf-8", "strict");
            if (str_language == NULL) {
                PyErr_SetString(PyExc_TypeError,
                                "Language could not be utf-8 encoded");
                goto exit_decref_language_unistr;
            }

            language = PyBytes_AsString(str_language);
        #endif

        if (language == NULL) {
            goto exit_decref_language_str;
        }
    }

    char *country = NULL;
    PyObject *unistr_country = Py_None;
    PyObject *str_country = Py_None;

    if (arg_country != Py_None) {
        unistr_country = PyUnicode_FromObject(arg_country);
        if (unistr_country == NULL) {
            PyErr_SetString(PyExc_TypeError,
                            "Country could not be converted to unicode");
        }

        #ifdef IS_PY3K
            // Python 3 encoding, supported by Python 3.3+

            country = PyUnicode_AsUTF8(unistr_country);

        #else
            // Python 2 encoding

            PyObject *str_country = PyUnicode_AsEncodedString(unistr_country, "utf-8", "strict");
            if (str_country == NULL) {
                PyErr_SetString(PyExc_TypeError,
                                "Country could not be utf-8 encoded");
                goto exit_decref_country_unistr;
            }

            country = PyBytes_AsString(str_country);
        #endif

        if (country == NULL) {
            goto exit_decref_country_str;
        }
    }
    
    address_parser_options_t options = LIBPOSTAL_ADDRESS_PARSER_DEFAULT_OPTIONS;
    options.language = language;
    options.country = country;

    address_parser_response_t *parsed = parse_address(input, options);
    if (parsed == NULL) {
        goto exit_decref_country_str;
    }

    result = PyList_New((Py_ssize_t)parsed->num_components);
    if (!result) {
        goto exit_destroy_response;
    }

    for (int i = 0; i < parsed->num_components; i++) {
        char *component = parsed->components[i];
        char *label = parsed->labels[i];
        PyObject *component_unicode = PyUnicode_DecodeUTF8((const char *)component, strlen(component), "strict");
        if (component_unicode == NULL) {
            Py_DECREF(result);
            goto exit_destroy_response;
        }

        PyObject *label_unicode = PyUnicode_DecodeUTF8((const char *)label, strlen(label), "strict");
        if (label_unicode == NULL) {
            Py_DECREF(component_unicode);
            Py_DECREF(result);
            goto exit_destroy_response;
        }
        PyObject *tuple = Py_BuildValue("(OO)", component_unicode, label_unicode);
        if (tuple == NULL) {
            Py_DECREF(component_unicode);
            Py_DECREF(label_unicode);
            goto exit_destroy_response;
        }

        // Note: PyList_SetItem steals a reference, so don't worry about DECREF
        PyList_SetItem(result, (Py_ssize_t)i, tuple);

        Py_DECREF(component_unicode);
        Py_DECREF(label_unicode);
    }

exit_destroy_response:
    address_parser_response_destroy(parsed);
exit_decref_country_str:
    #ifndef IS_PY3K
    if (str_country != Py_None) {
        Py_XDECREF(str_country);
    }
    #endif
exit_decref_country_unistr:
    if (unistr_country != Py_None) {
        Py_XDECREF(unistr_country);
    }
exit_decref_language_str:
    #ifndef IS_PY3K
    if (str_language != Py_None) {
        Py_XDECREF(str_language);
    }
    #endif
exit_decref_language_unistr:
    if (unistr_language != Py_None) {
        Py_XDECREF(unistr_language);
    }
exit_decref_input_str:
    #ifndef IS_PY3K
    Py_XDECREF(str_input);
    #endif
exit_decref_input_unistr:
    Py_XDECREF(unistr_input);

    return result;
}

static PyMethodDef parser_methods[] = {
    {"parse_address", (PyCFunction)py_parse_address, METH_VARARGS | METH_KEYWORDS, "parse_address(text, language, country)"},
    {NULL, NULL},
};



#ifdef IS_PY3K

static int parser_traverse(PyObject *m, visitproc visit, void *arg) {
    Py_VISIT(GETSTATE(m)->error);
    return 0;
}

static int parser_clear(PyObject *m) {
    Py_CLEAR(GETSTATE(m)->error);
    libpostal_teardown();
    libpostal_teardown_parser();
    return 0;
}

static struct PyModuleDef module_def = {
        PyModuleDef_HEAD_INIT,
        "_parser",
        NULL,
        sizeof(struct module_state),
        parser_methods,
        NULL,
        parser_traverse,
        parser_clear,
        NULL
};

#define INITERROR return NULL

PyObject *
PyInit_parser(void) {
#else

#define INITERROR return

void cleanup_libpostal(void) {
    libpostal_teardown();
    libpostal_teardown_parser(); 
}

void
init_parser(void) {
#endif

#ifdef IS_PY3K
    PyObject *module = PyModule_Create(&module_def);
#else
    PyObject *module = Py_InitModule("_parser", parser_methods);
#endif

    if (module == NULL) {
        INITERROR;
    }
    struct module_state *st = GETSTATE(module);

    st->error = PyErr_NewException("_parser.Error", NULL, NULL);
    if (st->error == NULL) {
        Py_DECREF(module);
        INITERROR;
    }

    if (!libpostal_setup() || !libpostal_setup_parser()) {
        PyErr_SetString(PyExc_TypeError,
                        "Error loading libpostal data");
    }

#ifndef IS_PY3K
    Py_AtExit(&cleanup_libpostal);
#endif


#ifdef IS_PY3K
    return module;
#endif
}

