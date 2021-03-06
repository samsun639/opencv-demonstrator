#include "mxml.hpp"
#include "cutil.hpp"
#include "pugixml.hpp"

#include <fstream>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <malloc.h>


namespace utils
{
namespace model
{

using namespace utils;


static void load_from_pugi_node(MXml &mx, pugi::xml_node node);
  

XmlAttribute::XmlAttribute(const XmlAttribute &a)
{
  *this = a;
}

XmlAttribute &XmlAttribute::operator =(const XmlAttribute &a)
{
  name = a.name;
  string_value = a.string_value;
  return *this;
}
        

MXml::MXml()
{
  name = "?";
}

MXml::MXml(const MXml &mx)
{
  *this = mx;
}

MXml &MXml::operator =(const MXml &mx)
{
  name        = mx.name;
  attributes  = mx.attributes;
  children    = mx.children;
  text        = mx.text;
  order       = mx.order;
  return *this;
}


std::string MXml::get_name() const
{
  return name;
}

bool MXml::has_child(std::string balise_name,
		    std::string att_name,
		    std::string att_value) const
{
  for(auto &child: children)
    if((child.name == balise_name)
       && (child.has_attribute(att_name))
       && (child.get_attribute(att_name).string_value == att_value))
      return true;
  return false;
}

MXml MXml::get_child(std::string balise_name,
		    std::string att_name,
		    std::string att_value)
{
  for(auto &child: children)
    if((child.name == balise_name)
       && (child.get_attribute(att_name).string_value == att_value))
	return child;

  log_anomaly(main_log, "XML child not found: %s in %s where %s = %s.",
	      balise_name.c_str(), name.c_str(),
	      att_name.c_str(), att_value.c_str());
  return MXml();
}

bool MXml::has_child(std::string name) const
{
  for(auto &ch: children)
    if(ch.name == name)
      return true;
  return false;
}

MXml MXml::get_child(std::string name) const
{
  for(auto &ch: children)
    if(ch.name == name)
      return ch;
  log_anomaly(main_log, "Child not found: %s in %s.",
	      name.c_str(), this->name.c_str());
  return MXml();
}

 
void MXml::get_children(std::string name,
			std::vector<const MXml *> &res) const
{
  for(auto &ch: children)
  {
    if(ch.name == name)
      res.push_back(&ch);
  }
}

std::vector<MXml> MXml::get_children(std::string name) const
{
  std::vector<MXml> res;
  for(auto &ch: children)
  {
    if(ch.name == name)
      res.push_back(ch);
  }
  return res;
}

bool MXml::has_attribute(std::string name) const
{
  for(auto &att: attributes)
    if(att.name == name)
      return true;
  return false;
}

XmlAttribute MXml::get_attribute(std::string name) const
{
  for(auto &att: attributes)
    if(att.name == name)
      return att;
  log_anomaly(main_log, "getAttribute(%s): attribute not found in %s.",
	      name.c_str(), this->name.c_str());
  return XmlAttribute();
}

MXml::MXml(std::string name, std::vector<XmlAttribute> *attributes, std::vector<MXml> *children)
{
  this->name = name;
  this->attributes = *attributes;
  delete attributes;
  this->children = *children;
  delete children;
}



static void load_from_pugi_node(MXml &mx, pugi::xml_node node)
{
  mx.name = node.name();
  for(auto att: node.attributes())
  {
    XmlAttribute xatt;
    xatt.name = att.name();
    xatt.string_value = att.value();
    mx.attributes.push_back(xatt);
  }
  for(auto ch: node.children())
  {
    auto type = ch.type();
    if(type == pugi::xml_node_type::node_pcdata)
    {
      mx.add_text(ch.text().get());
    } 
    else if((type == pugi::xml_node_type::node_element) 
	    || (type == pugi::xml_node_type::node_element))
    {
      MXml child;
      load_from_pugi_node(child, ch);
      mx.add_child(child);
    }
  }
}

int MXml::from_file(std::string filename)
{
  pugi::xml_document doc;
  auto result = doc.load_file(filename.c_str());
  if(result.status != pugi::xml_parse_status::status_ok)
  {
    log_anomaly(main_log, "Error occurred while parsing [%s]: %s.",
		filename.c_str(), result.description());
    return -1;
  }

  auto elt = doc.document_element();
  //elt.print(std::cout);
  load_from_pugi_node(*this, elt);
  return 0;
}

int MXml::from_string(std::string s)
{
  pugi::xml_document doc;
  auto result = doc.load_buffer(s.c_str(), s.size());
  if(result.status != pugi::xml_parse_status::status_ok)
  {
    log_anomaly(main_log, "Error occurred while parsing XML string: %s.",
		result.description());
    return -1;
  }

  auto elt = doc.document_element();
  load_from_pugi_node(*this, elt);
  return 0;
}


void MXml::add_child(const MXml &mx)
{
  order.push_back(true);
  children.push_back(mx);
}

void MXml::add_text(std::string s)
{
  order.push_back(false);
  //this corrupts utf8 if it contains Cyrillic text. 
  //text.push_back(str::latin_to_utf8(s));
  text.push_back(s);

}

std::string MXml::dump_content() const
{
  std::string res = "";
  int index_el = 0;
  int index_tx = 0;
  for(unsigned int i = 0; i < order.size(); i++)
  {
    if(order[i])
      res += children[index_el++].dump();
    else
      res += text[index_tx++];
  }
  const char *s = res.c_str();
  bool only_spaces = true;
  for(unsigned int i = 0; i < strlen(s); i++)
  {
    if(s[i] != ' ')
    {
      only_spaces = false;
      break;
    }
  }
  if(only_spaces)
    return "";
  return res;
}

std::string MXml::dump() const
{
  std::string res = "<" + name;

  if((attributes.size() == 0) && (order.size() == 0))
    return res + "/>";

  for (auto &att: attributes) 
    res += " " + att.name + "=\"" + att.string_value + "\"";
  
  res += ">";
  int index_el = 0;
  int index_tx = 0;
  for(unsigned int i = 0; i < order.size(); i++)
  {
    if(order[i])
      res += children[index_el++].dump();
    else
      res += text[index_tx++];
  }
  res += "</" + name + ">";
  return res;
}

XmlAttribute::XmlAttribute()
{
  this->string_value = "?";
  name = "?";
}

XmlAttribute::XmlAttribute(std::string name, std::string value)
{
  this->string_value = str::latin_to_utf8(value);
  this->name = name;
}

int XmlAttribute::to_int() const
{
  char temp[20];
  sprintf(temp, "%s", string_value.c_str());
  int val = -1;
  sscanf(temp, "%d", &val);
  return val;
}

std::string XmlAttribute::to_string()  const
{
  const char *s = string_value.c_str();
  char buf[1000];
  unsigned int n = strlen(s);
  unsigned int j = 0;
  for(unsigned int i = 0; i < n; i++)
  {
    if((s[i] == '\\') && (i < n - 1) && (s[i+1] == 'G'))
    {
      buf[j++] = '"';
      i++;
    }
    else
    {
      buf[j++] = s[i];
    }
  }
  buf[j] = 0;
  return std::string(buf);
}

bool XmlAttribute::to_bool() const
{
  return string_value == "true";
}

double XmlAttribute::to_double() const
{
  char temp[30];
  sprintf(temp, "%s", string_value.c_str());
  float val = -1;
  sscanf(temp, "%f", &val);
  return val;
}


void yyerror(const char *s)
{
    //printf("Error : %s\n", s);
}

std::string MXml::xml_string_to_ascii(std::string s)
{
    char *res = (char *) malloc(s.size()+1);
    const char *s2 = s.c_str();
    unsigned int di = 0;
    for(unsigned int i = 0; i < strlen(s2); i++)
    {
        if(((i+4) < strlen(s2)) && (s2[i] == '&') && (s2[i+1] == 'a') && (s2[i+2] == 'm') && (s2[i+3] == 'p') && (s2[i+4] == ';'))
        {
            i += 4;
            res[di++] = '&';
        }
        else if(((i+1) < strlen(s2)) && (s2[i] == '\\') && (s2[i+1] == '\\'))
        {
            i++;
            res[di++] = '\n';
        }
        else
            res[di++] = s2[i];
    }
    res[di] = 0;
    std::string sres = std::string(res);
    free(res);
    return sres;
}

std::string MXml::ascii_string_to_xml(std::string s)
{
    char *res = (char *) malloc(s.size()+100);
    const char *s2 = s.c_str();
    unsigned int di = 0;
    for(unsigned int i = 0; i < strlen(s2); i++)
    {
        if(s2[i] == '&')
        {
            res[di++] = '&';
            res[di++] = 'a';
            res[di++] = 'm';
            res[di++] = 'p';
            res[di++] = ';';
        }
        else if(s2[i] == '\n')
        {
            res[di++] = '\\';
            res[di++] = '\\';
        }
        else
            res[di++] = s2[i];
    }
    res[di] = 0;
    std::string sres = std::string(res);
    free(res);
    return sres;
}

}
}

