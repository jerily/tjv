package require tjv

tjv::compile -type object -properties {
  {pathParameters -type "object" -properties {
    {user_id -type "integer" -required}
  }}
  {queryParameters -type "object" -properties {
    {age -type "integer" -minimum 0}
    {abc -type "object" -properties {
      {foo -type "string" -required}
      {bar -type "string" -pattern "someregexp"}
    }}
  }}
  {body -type "json" -properties {
    {age -type "integer" -minimum 0}
    {abc -type "object" -properties {
      {foo -type "string" -required}
      {bar -type "string" -pattern "someregexp"}
    }}
  }}
  {foo -type "email"}
} vvv

puts "\n\nvalidate it\n\n"

$vvv validate {
    pathParameters {
        user_id 10
    }
    queryParameters {
        age 10
        abc {
            foo "123"
            bar "444"
        }
    }
    body {{
        "age": 10,
        "abc": {
            "foo": "bla",
            "bar": "qux"
        }
    }}
    foo "emailhere"
} error_var

puts "\n\nERROR VAR: $error_var"
