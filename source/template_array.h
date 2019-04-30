#if !defined template_array_count_type 
#    define template_array_count_type usize
#endif

#ifdef template_array_is_buffer

struct template_array_name {
    
#ifdef template_array_static_count
    
    template_array_data_type Base[template_array_static_count];
    static const template_array_count_type Capacity = template_array_static_count; 
    
#else
    
    template_array_data_type *Base;
    template_array_count_type Capacity;
    
#endif
    
    template_array_count_type Count;
    
    template_array_data_type & operator[] (template_array_count_type Index) {
        assert(Index < Count);
        return Base[Index];
    } 
};


template_array_data_type * Push(template_array_name *buffer){
    if (buffer->Count < buffer->Capacity) {
        auto result = buffer->Base + ((buffer->Count)++);
        
        return result;
    }
    
    return NULL;
}


#else

struct template_array_name {
    template_array_data_type *Base;
    union {
        template_array_count_type Capacity, Count;
    };
    
    template_array_data_type & operator[] (template_array_count_type Index) {
        assert(Index < Count);
        return Base[Index];
    }
};

#endif

#undef template_array_name
#undef template_array_data_type
#undef template_array_count_type

#ifdef template_array_static_count
#undef template_array_static_count
#endif

#ifdef template_array_is_buffer
#undef template_array_is_buffer
#endif
