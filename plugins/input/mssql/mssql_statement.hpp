#ifndef MSSQL_STATEMENT_HPP
#define MSSQL_STATEMENT_HPP

#include "mssql_connection.hpp"
#include "odbc_exception.hpp"

#ifdef _WINDOWS
#include <windows.h>
#endif // _WINDOWS
#include <sql.h>
#include <sqlext.h>


#include <string>
#include <vector>

class mssql_statement
{
    struct column_info
    {
        column_info()
        : name(),
        native_type(),
        sql_type(0),
        sql_size(0),
        sql_scale(0),
        sql_nullable(0),
        c_type(0),
        c_len(0),
        buffer(0),
        len_or_ind(0)
        {
        }
        ~column_info()
        {
            delete[] buffer;
        }
        std::string name;
        std::string native_type;
        SQLSMALLINT sql_type;
        SQLULEN sql_size;
        SQLSMALLINT sql_scale;
        SQLSMALLINT sql_nullable;
        SQLSMALLINT c_type;
        SQLULEN c_len;
        char* buffer;
        SQLLEN len_or_ind;
    };
    
public:
    mssql_statement(mssql_connection& con, const std::string& sql)
    : stmt_(0),
		  sql_(sql)
    {
        SQLRETURN rc = SQLAllocHandle(SQL_HANDLE_STMT, con._get_connection_handle(), &stmt_);
        if (!SQL_SUCCEEDED(rc))
            throw odbc_exception(rc, SQL_HANDLE_DBC, con._get_connection_handle());
    }
    
    void execute()
    {
        SQLRETURN rc;
        rc = SQLExecDirect(stmt_, (SQLCHAR*)sql_.c_str(), SQL_NTS);
        if (!SQL_SUCCEEDED(rc))
            throw odbc_exception(rc, SQL_HANDLE_STMT, stmt_);
        
        rc = SQLNumResultCols(stmt_, &cols_);
        if (!SQL_SUCCEEDED(rc))
            throw odbc_exception(rc, SQL_HANDLE_STMT, stmt_);
        
        column_info_.resize(cols_);
        for (int i = 0; i < cols_; i++)
        {
            column_info& col = column_info_[i];
            SQLCHAR c_name[256];
            SQLSMALLINT len;
            rc = SQLDescribeCol(
                stmt_,
                i + 1,
                c_name,
                sizeof(c_name),
                &len,
                &col.sql_type,
                &col.sql_size,
                &col.sql_scale,
                &col.sql_nullable
            );
            if (!SQL_SUCCEEDED(rc))
                throw odbc_exception(rc, SQL_HANDLE_STMT, stmt_);
            
            col.name.assign(reinterpret_cast<char*>(c_name));
            
            rc = SQLColAttribute(
                stmt_,
                i + 1,
                SQL_DESC_TYPE_NAME,
                c_name,
                sizeof(c_name),
                &len,
                NULL
            );
            if (!SQL_SUCCEEDED(rc))
                throw odbc_exception(rc, SQL_HANDLE_STMT, stmt_);
            
            col.native_type.assign(reinterpret_cast<char*>(c_name));
            
            switch (col.sql_type)
            {
                case SQL_WCHAR:
                case SQL_WVARCHAR:
                case SQL_WLONGVARCHAR:
                    col.c_type = SQL_C_WCHAR;
                    break;
                default:
                    col.c_type = SQL_C_CHAR;
                    break;
            }
            col.buffer = new char[256];
            rc = SQLBindCol(
                stmt_,
                i + 1,
                col.c_type,
                nullptr,
                0,
                &col.len_or_ind
            );
            if (!SQL_SUCCEEDED(rc))
                throw odbc_exception(rc, SQL_HANDLE_STMT, stmt_);
        }
    }
    
    bool fetch()
    {
        SQLRETURN rc = SQLFetch(stmt_);
        if SQL_SUCCEEDED(rc)
            return true;
        if (rc != SQL_NO_DATA)
            throw odbc_exception(rc, SQL_HANDLE_STMT, stmt_);
        
        return false;
    }
    
    bool is_null(int i)
    {
        column_info& col = column_info_[i];
        return (col.len_or_ind == SQL_NULL_DATA);
    }
    
    int get_int(int i)
    {
        return 0;
    }
    
    double get_double(int i)
    {
        return 0;
    }
    
    const char* get_char(int i)
    {
        return 0;
    }
    
    short columns()
    {
        return cols_;
    }
    
    std::string column_name(int i)
    {
        return column_info_[i].name;
    }
    
    ~mssql_statement()
    {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt_);
    }
private:
    std::vector<column_info> column_info_;
    SQLSMALLINT cols_;
    std::string sql_;
    SQLHSTMT stmt_;
};

#endif // MSSQL_STATEMENT_HPP