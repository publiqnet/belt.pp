#include "generator.hpp"

#include <cassert>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <exception>
#include <utility>
#include<iostream>
#include<string>

using std::cout;
using std::endl;
using std::string;
using std::vector;
using std::unordered_map;
using std::unordered_set;
using std::pair;
using std::runtime_error;

state_holder::state_holder()
    : namespace_name()
    , map_types{{"String", "string"},
                {"Bool", "bool"},
                {"Int8", "int"},
                {"UInt8", "int"},
                {"Int16", "int"},
                {"UInt16", "int"},
                {"Int", "int"},
                {"Int32", "int"},
                {"UInt32", "int"},
                {"Int64", "int"},
                {"UInt64", "int"},
                {"Float32", "float"},
                {"Float64", "double"},
                {"TimePoint", "integer"},
                {"Object", "::beltpp::packet"},
                {"Extension", "::beltpp::packet"}}
{
}

namespace
{
enum g_type_info {
                type_empty = 0x0,
                type_simple = 0x1,
                type_object=0x2,
                type_extension=0x4,
                type_simple_object = type_simple | type_object,
                type_simple_extension = type_simple | type_extension,
                type_object_extension = type_object | type_extension,
                type_simple_object_extension = type_simple | type_object_extension
};

string handleArrayForPrimitives(int count, string member_name)
{

    string arrayCase;
    string item = member_name + "Item";
    arrayCase +=
               "          foreach ($data->" + member_name + " as $" + item + ") { \n";

    for( int i=1; i!= count; i++)
    {
        arrayCase +=
                   "          foreach ($" + item + " as $" + item + std::to_string(i) + ") { \n";
        item = item + std::to_string(i);
    }
        arrayCase +=
                  "            $this->add" + ((char)( member_name.at(0)-32 ) + member_name.substr( 1, member_name.length()-1 )) + "($" + item + ");\n";

    for( int i=1; i!= count; i++)
    {
        arrayCase +=
                   "           } \n";
    }
        arrayCase +=
                   "           } \n";
        return arrayCase;
}

string handleArrayForObjects(int count, string member_name, string object_name)
{

        string item = member_name +"Item";
        string arrayCase;
             arrayCase+=
                       "          foreach ($data->" + member_name + " as $" + item + ") { \n";

        for( int i=1; i!= count; i++)
        {
            arrayCase +=
                       "          foreach ($" + item + " as $" + item + std::to_string(i) + ") { \n";
            item = item + std::to_string(i);
        }
            arrayCase +=
                       "              $" + item + "Obj = new " + object_name + "(); \n"
                       "              $" + item + "Obj->validate($" + item + "); \n";

        for( int i=1; i!= count; i++)
        {
            arrayCase +=
                       "           } \n";
        }
            arrayCase +=
                       "           } \n";
            return arrayCase;

}

void handleHashForPrimitives(string info[], string& setFunction, string& hashCase, string member_name)
{
    setFunction +=
     "    /**\n"
     "    * @var string\n"
     "    */\n"
     "    private $" + member_name + "Key;\n"
     "    /**\n"
     "    * @param string  $" + member_name + "Key\n"
     "    */\n"
     "    public function set" + member_name + "Key(string $key)\n"
     "    {\n"
     "        $this->" + member_name + "Key = $key;\n"
     "    }\n"

     "    /**\n"
     "    * @var " + info[3] + "\n"
     "    */\n"
     "    private $" + member_name + "Value;\n"
     "    /**\n"
     "    * @param " + info[3] + " $" + member_name + "Value\n"
     "    */\n"
     "    public function set" + member_name + "Value(" + info[3] + " $value)\n"
     "    {\n"
     "        $this->" + member_name + "Value = $value;\n"
     "    }\n";

  hashCase +=
        "        foreach ($data->hash as $key => $value) {\n"
        "            $this->set" + member_name + "Key($key);\n"
        "            $this->set" + member_name + "Value($value);\n"
        "        }\n";
}

void handleHashForObjects(string info[], string& setFunction, string& hashCase, string member_name)
{

        setFunction +=
                "    /**\n"
                "    * @var string\n"
                "    */\n"
                "    private $" + member_name + "Key;\n"
                "    /**\n"
                "    * @param string  $" + member_name + "Key\n"
                "    */\n"
                "    public function set" + member_name + "Key(string $key)\n"
                "    {\n"
                "        $this->" + member_name + "Key = $key;\n"
                "    }\n";

        string ObjType = info[3];
        if(info[3] == "::beltpp::packet")
            ObjType = "Object";
          hashCase +=
                    "        foreach ($data->hash as $key => $value) {\n"
                    "            $this->set" + member_name + "Key($key);\n"
                    "            $hashItemObj = new " + ObjType + "();\n"
                    "            $hashItemObj->validate($value);\n"
                    "         }\n";

}

string convert_type(string const& type_name, state_holder& state, g_type_info& type_detail)
{
    type_detail = type_empty;
    if ("Object" == type_name)
        type_detail = type_object;
    else if ("Extension" == type_name)
        type_detail = type_extension;
    else
        type_detail = type_simple;

    auto it_type_name = state.map_types.find(type_name);
    if (it_type_name != state.map_types.end())
        return it_type_name->second;
    return type_name;
}

void construct_type_name(expression_tree const* member_type,
                            state_holder& state,
                            g_type_info& type_detail,
                            string* result)
{
    if (member_type->lexem.rtt == identifier::rtt)
    {
        result[0] = convert_type(member_type->lexem.value, state, type_detail);
    }
    else if (member_type->lexem.rtt == keyword_array::rtt &&
             member_type->children.size() == 1 )
    {

        result[0] = "array";
        int count = 1;
        auto it = member_type->children.front();
        for(; it->lexem.rtt != identifier::rtt; it = it->children.front())
        {
            count++;
        }
        if(it->lexem.rtt == identifier::rtt)
        {
            result[1] = convert_type(it->lexem.value, state, type_detail);
        }
        result[2] = std::to_string(count);
    }
    else if (member_type->lexem.rtt == keyword_hash::rtt &&
             member_type->children.size() == 2 )
    {
        int count = 1;
        auto it = member_type->children.front();
        for(; it->lexem.rtt != identifier::rtt; it = it->children.front())
        {
            count++;
        }
        if(it->lexem.rtt == identifier::rtt)
        {
            result[1] = convert_type(it->lexem.value, state, type_detail);
        }

        result[0] = "hash";
        result[2] = std::to_string(count);
        result[3] = convert_type(member_type->children.back()->lexem.value, state, type_detail);

    }
    else
        throw runtime_error("can't get type definition, wtf!");
    }
}

string analyze(state_holder& state,
               expression_tree const* pexpression)
{
    size_t rtt = 0;
    string result=R"file_template(
interface Validator
{
    public function validate(stdClass $data);
}
trait RttSerializableTrait
{
    public function jsonSerialize()
    {
        $vars = get_object_vars($this);
        $vars['rtt'] = array_search(static::class, Rtt::types);

        return $vars;
     }
}

trait RttToJsonTrait
{
    public function convertToJson()
    {
        return json_encode($this);
    }
}

class Rtt
{
    CONST types = [)file_template";

    assert(pexpression);

    unordered_map<size_t, string> class_names;
    string resultMid;
    if (pexpression->lexem.rtt != keyword_module::rtt ||
        pexpression->children.size() != 2 ||
        pexpression->children.front()->lexem.rtt != identifier::rtt ||
        pexpression->children.back()->lexem.rtt != scope_brace::rtt ||
        pexpression->children.back()->children.empty())
        throw runtime_error("wtf");
    else
    {
        string module_name = pexpression->children.front()->lexem.value;
        state.namespace_name = module_name;

        for (auto item : pexpression->children.back()->children)
        {
            if (item->lexem.rtt == keyword_class::rtt)
            {
                if (item->children.size() != 2 ||
                    item->children.front()->lexem.rtt != identifier::rtt ||
                    item->children.back()->lexem.rtt != scope_brace::rtt)
                    throw runtime_error("type syntax is wrong");

                string type_name = item->children.front()->lexem.value;
                resultMid += analyze_struct(state,
                                         item->children.back(),
                                         type_name);
                class_names.insert(std::make_pair(rtt, type_name));

            }
            ++rtt;
        }
    }

    if (class_names.empty())
        throw runtime_error("wtf, nothing to do");

    size_t max_rtt = 0;
    for (auto const& class_item : class_names)
    {
        if (max_rtt < class_item.first)
            max_rtt = class_item.first;
    }

    result+="\n";

    for (size_t index = 0; index < max_rtt + 1; ++index)
    {
        if(!(class_names[index].empty()))
            result+="\t"+std::to_string(index)+" => '"+class_names[index]+"',\n";

    }
    result += R"file_template(
    ];

    /**
    * @param string|object $jsonObj
    * @return bool|string
    */
    public static function validate($jsonObj)
    {
        if (!is_object($jsonObj)) {
            $jsonObj = json_decode($jsonObj);
            if ($jsonObj === null) {
                return false;
                          }
            }

        if (!isset($jsonObj->rtt)) {
            return false;
        }

        if (!isset(Rtt::types[$jsonObj->rtt])) {
            return false;
        }
        try {
            $className = Rtt::types[$jsonObj->rtt];
            /**
            * @var Validator $class
            */
            $class = new $className;
            $class->validate($jsonObj);
            return true;
        } catch (Throwable $e) {
            return $e->getMessage();
        }
    }
}
)file_template";

    return result+resultMid;
}

string analyze_struct(state_holder& state,
                      expression_tree const* pexpression,
                      string const& type_name)
{
    if (state.namespace_name.empty())
        throw runtime_error("please specify package name");

    assert(pexpression);

    vector<pair<expression_tree const*, expression_tree const*>> members;

    if (pexpression->children.size() % 2 != 0)
        throw runtime_error("inside class syntax error, wtf - " + type_name);

    auto it = pexpression->children.begin();
    for (size_t index = 0; it != pexpression->children.end(); ++index, ++it)
    {
        auto const* member_type = *it;
        ++it;
        auto const* member_name = *it;

        if (member_name->lexem.rtt != identifier::rtt)
            throw runtime_error("inside class syntax error, wtf, still " + type_name);

        members.push_back(std::make_pair(member_name, member_type));
    }

    string result;
    string params;
    string setFunction;
    string getFunction;
    string addFunction;
    string arrayCase;
    string trivialTypes;
    string objectTypes;
    string mixedTypes;
    string hashCase;

    result += "class " + type_name + " implements Validator, JsonSerializable\n";
    result += "{\n";
    result += "    use RttSerializableTrait;\n"
              "    use RttToJsonTrait;\n";

    for (auto member_pair : members)
    {
        auto const& member_name = member_pair.first->lexem;
        auto const& member_type = member_pair.second;
        if (member_name.rtt != identifier::rtt)
            throw runtime_error("use \"variable type\" syntax please");

        g_type_info type_detail;

        string info[4];
        construct_type_name(member_type, state, type_detail, info);

        if (info[0] == "::beltpp::packet")
        {

            params +=
                    "    /**\n"
                    "    * @var mixed \n"
                    "    */ \n"
                    "    private $" + member_name.value + ";\n";

            mixedTypes +=
                    "            Rtt::validate($data->" +  member_name.value + ");\n";
        }
        else if(info[0] == "hash")
        {
            params +=
                    "    /**\n"
                    "    * @var array \n"
                    "    */ \n"
                    "    private $" + member_name.value + ";\n";
        }
        else
        {
            params +=
                    "    /**\n"
                    "    * @var "+ info[0] + "\n" +
                    "    */ \n" +
                    "    private $" + member_name.value + ";\n";
        }

        if(     info[0] == "array" &&
                ( info[1] != "int" &&
                  info[1] != "string" &&
                  info[1] != "bool" &&
                  info[1] != "float" &&
                  info[1] != "double" &&
                  info[1] != "integer"
                )
           )
        {
            arrayCase += handleArrayForObjects(std::stoi(info[2]), member_name.value, info[1]);

        }
        else if (
                 (              info[0] == "array" &&
                                (  info[1] == "int" ||
                                   info[1] == "string" ||
                                   info[1] == "bool" ||
                                   info[1] == "float" ||
                                   info[1] == "double" ||
                                   info[1] == "integer"
                                )
                    )
                 )
        {

            string item = member_name.value+"Item";
            addFunction +=
                         "    /**\n"
                         "    * @param " + info[1] + " $" + item +"\n"
                         "    */\n"
                         "    public function add" + ((char)( member_name.value.at(0)-32 ) + member_name.value.substr( 1, member_name.value.length()-1 ))  +  "(" + info[1] + " $" + item + ")\n"
                         "    {\n"
                         "        $this->" + member_name.value + "[] = $" + item + ";\n"
                         "    }\n";


            arrayCase += handleArrayForPrimitives(std::stoi(info[2]), member_name.value);

        }

        if(     info[0] == "hash" &&
                ( info[3] != "int" &&
                  info[3] != "string" &&
                  info[3] != "bool" &&
                  info[3] != "float" &&
                  info[3] != "double" &&
                  info[3] != "integer"
                )
           )
        {

            handleHashForObjects(info, setFunction, hashCase, member_name.value);

        }
        else if (
                 (              info[0] == "hash" &&
                                (  info[3] == "int" ||
                                   info[3] == "string" ||
                                   info[3] == "bool" ||
                                   info[3] == "float" ||
                                   info[3] == "double" ||
                                   info[3] == "integer"
                                )
                    )
                 )
        {
             handleHashForPrimitives(info, setFunction, hashCase, member_name.value);

        }


        else if (
                 info[0] == "int" ||
                 info[0] == "string" ||
                 info[0] == "bool" ||
                 info[0] == "float" ||
                 info[0] == "double" ||
                 info[0] == "integer"
                 )
        {
            trivialTypes +=
                          "          $this->set" + ((char)(member_name.value.at(0)-32) + member_name.value.substr( 1,member_name.value.length()-1) ) + "($data->" + member_name.value + "); \n";

            string type;
            if (info[0] == "integer")
                type = "int";
            else
                type = info[0];

            setFunction +=
                         "    /** \n"
                         "    * @param " + type + " $" + member_name.value + "\n"
                         "    */ \n"
                         "    public function set" + (char)( member_name.value.at(0)-32 ) + member_name.value.substr( 1,member_name.value.length()-1 ) + "(" + type +" $" + member_name.value + ") \n"
                         "    { \n"
                         "            $this->" + member_name.value + " = " ;

            if (!(info[0] == "integer"))
                setFunction += "$";

            if (info[0] == "integer")
                setFunction += "strtotime($";

            setFunction += member_name.value;

            if (info[0] == "integer")
                setFunction += ")";

            setFunction +="; \n"
                        "    } \n";
        }
        else if( !(info[0] == "::beltpp::packet") )
        {
            objectTypes +=
                         "        $" + member_name.value + "Obj = new "+ info[0] + "();\n"
                         "        $" + member_name.value + "Obj -> validate($data-> "+ member_name.value  + ");\n";
        }

        getFunction +=
                    "    public function get" + ((char)( member_name.value.at(0)-32 ) + member_name.value.substr( 1, member_name.value.length()-1 )) + "() \n"
                    "    {\n"
                    "        return $this->" + member_name.value + ";\n"
                    "    }\n";
    }

    string  validation =
                       "    public function validate(stdClass $data) \n"
                       "    { \n"
                                + objectTypes + trivialTypes + arrayCase + mixedTypes + hashCase +
                       "    } \n";

    result += params + setFunction + getFunction + addFunction + validation;
    result += "} \n";

    return result;
}
