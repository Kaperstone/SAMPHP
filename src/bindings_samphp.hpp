#define ARG_OFFSET 3
PHP_FUNCTION(CallAMXNative)
{
	int argc = ZEND_NUM_ARGS();
    zval **args;
	AMX *amx = sampgdk_fakeamx_amx();

    args = (zval **)safe_emalloc(argc, sizeof(zval *), 0);
    
    if(argc < 2 ||
        zend_get_parameters_array(argc, argc, args) == FAILURE)
    {
        efree(args);
        WRONG_PARAM_COUNT;
    }
	
	convert_to_string(args[0]);
	convert_to_string(args[1]);

	const char *function_name = Z_STRVAL_P(args[0]);
	const char *function_return = Z_STRVAL_P(args[1]);
	
	AMX_NATIVE native = sampgdk_FindNative(function_name);
	
	if(native == 0)
	{
        efree(args);
		std::cerr << "Native not found: " << function_name << std::endl;
		RETURN_BOOL(false);
	}

	cell *params = NULL;
	const char *parameter_descriptors = NULL;
	int num_descriptors = 0;

	if(argc > 2)
	{
		convert_to_string(args[2]);
		if(Z_STRLEN_P(args[2]) > 0)
		{
			num_descriptors = argc - ARG_OFFSET;
			parameter_descriptors = Z_STRVAL_P(args[2]);

			params = new cell[Z_STRLEN_P(args[2]) + 1];
			params[0] = Z_STRLEN_P(args[2]) * sizeof(cell);
			
			int pd_i = 0;
			for(int p_i = ARG_OFFSET; p_i < argc; p_i++)
			{
				zval *val = args[p_i];
			
				switch(parameter_descriptors[pd_i])
				{
				case 's':
					convert_to_string(val);		
					sampgdk_fakeamx_push_string(Z_STRVAL_P(val), NULL, &params[pd_i + 1]);			
					break;
				case 'l':
					convert_to_long(val);
					params[pd_i + 1] = (cell) Z_LVAL_P(val);
					break;
				case 'd':
					convert_to_double(val);
					params[pd_i + 1] = amx_ftoc(Z_DVAL_P(val));

					break;
				case 'u':	// String reference object
				{
					//get buffer size from next param:
					zval *size_val = args[p_i + 1];
					convert_to_long(size_val);
					int size = Z_LVAL_P(size_val);
					// Push size cells as a buffer
					sampgdk_fakeamx_push(size, &params[pd_i + 1]);
					break;
				}
				case 'v':	//Float reference
					//push 1 cell for the float
					sampgdk_fakeamx_push(1, &params[pd_i + 1]);
					break;
				case 'w':
					//push 1 cell for the integer
					sampgdk_fakeamx_push(1, &params[pd_i + 1]);
					break;
				default:
					efree(args);
					delete params;

					std::cerr << "UNKNOWN DESCRIPTOR: " << parameter_descriptors[pd_i] << std::endl;

					RETURN_BOOL(false);
					break;
				}

				pd_i++;
			}
		}
	}
	//Call function
	cell retval = sampgdk::CallNative(native, params);
	
	// Clean up and reference hydration
	for(int i = 0; i < num_descriptors; i++)
	{
		switch(parameter_descriptors[i])
		{
			case 's':
				sampgdk_fakeamx_pop(params[i + 1]);
				break;
			case 'u':
			{
				zval *ref = args[ARG_OFFSET + i];
				int size = (int) params[i + 2];
			
				char *buffer = new char[size];

				sampgdk_fakeamx_get_string(params[i + 1], buffer, size);
				sampgdk_fakeamx_pop(params[i + 1]);

				convert_to_null(ref);
			
				ZVAL_STRING(ref, buffer, true);
				delete buffer;
				break;
			}
			case 'v':
			{
				zval *ref = args[ARG_OFFSET + i];
				float tmp;
				cell retval;
				sampgdk_fakeamx_get_cell(params[i + 1], &retval);
				sampgdk_fakeamx_pop(params[i + 1]);

				tmp = amx_ctof(retval);

				convert_to_null(ref);
				ZVAL_DOUBLE(ref, tmp);
			
				break;
			}
			case 'w':
			{
				zval *ref = args[ARG_OFFSET + i];
				int tmp;
				cell retval;
				sampgdk_fakeamx_get_cell(params[i + 1], &retval);
				sampgdk_fakeamx_pop(params[i + 1]);

				tmp = (int) retval;
	
				convert_to_null(ref);
				ZVAL_LONG(ref, tmp);
			
				break;
			}
		}
	}


	
	// Return value
	char ret_type = function_return[0];

    efree(args);
	delete params;

	switch(ret_type)
	{
	case 'l':
		RETURN_LONG(retval);
	case 'd':
		RETURN_DOUBLE(amx_ctof(retval));
	case 'n':
		RETURN_NULL();

	case 's':
		
	default:
		std::cerr << "UNKNOWN RETURN TYPE: " << ret_type << std::endl;

		RETURN_BOOL(false);
		break;
	}
   
}

PHP_FUNCTION(AMXNativeExists) {
	char* function_name;
	int function_lenght;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &function_name, &function_lenght) != SUCCESS) {
		RETURN_NULL();
	}
	RETURN_BOOL(sampgdk::FindNative(function_name) != NULL);
}

PHP_FUNCTION(GetAMXNativesList) {
	if (ZEND_NUM_ARGS() != 0)
		WRONG_PARAM_COUNT;
	
	int number;
	const AMX_NATIVE_INFO* amx = sampgdk::GetNatives(number);
	array_init(return_value);
	for (int i = 0; i < number; i++) {
		add_next_index_string(return_value, amx[i].name, 1);
	}
}

PHP_FUNCTION(samphpversion) {
	if (ZEND_NUM_ARGS() != 0)
		WRONG_PARAM_COUNT;

	char* str = estrdup(SAMPHPVERSION);
	RETURN_STRING(str, 0);
}

PHP_FUNCTION(sampgdkversion) {
	if (ZEND_NUM_ARGS() != 0)
		WRONG_PARAM_COUNT;

	char* str = estrdup(sampgdk::GetVersionString());
	RETURN_STRING(str, 0);
}

PHP_FUNCTION(ToggleDebugging) {
	if (ZEND_NUM_ARGS() != 1)
		WRONG_PARAM_COUNT;

	bool toggle;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "b", &toggle) != SUCCESS)
		RETURN_NULL();

	samphp::instance->DEBUG = toggle;
	RETURN_BOOL(toggle);
}