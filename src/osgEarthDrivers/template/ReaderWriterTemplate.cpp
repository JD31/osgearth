#include <osg/Notify>
#include <osgDB/ReaderWriter>
#include <osgDB/FileUtils>
#include <osgDB/FileNameUtils>
#include <osgDB/Registry>

#include <osgEarth/URI>
#include <osgEarth/Registry>

#include "NLTemplate.h"

using namespace NL::Template;

/**
* OpenSceneGraph psuedoloader that runs a node through the NLTemplate (https://github.com/catnapgames/NLTemplate)
* templating library before actually loading it. The original goal of this plugin was to simplify 
* the management of complicated earth files but it can be used to process any text based format.
* 
* To run your file through the template processor, simply append .template to your filename.
* osgviewer map.earth.template
* 
* This will process any {% include file.xml %} snippits in the text.
*
* To provide context variables to the templating engine provide an Options string with a list of key value pairs
* separated by the equal sign.
* osgviewer map.earth.template -O "layer=123 max_range=1e6 shapefile=world.shp"
*/
class TemplateReaderWriter: public osgDB::ReaderWriter
{
public:
    TemplateReaderWriter()
    {
        supportsExtension("template","Template");
    }

    virtual const char* className() const { return "TemplateReaderWriter"; }

    virtual bool acceptsExtension(const std::string& extension) const
    {
        return osgDB::equalCaseInsensitive(extension,"template");
    }

    virtual ReadResult readObject(const std::string& file_name, const osgDB::Options* options) const
    {
        return readNode( file_name, options );
    }

    virtual ReadResult readNode(const std::string& fileName, const osgDB::Options* options) const
    {
        std::string ext = osgDB::getFileExtension( fileName );
        if ( !acceptsExtension( ext ) )
            return ReadResult::FILE_NOT_HANDLED;

        std::string realName = osgDB::getNameLessExtension( fileName );
        if (!osgDB::fileExists(realName))
        {
            return ReadResult::FILE_NOT_FOUND;
        }

        std::string realExt = osgDB::getFileExtension( realName );
        osg::ref_ptr< osgDB::ReaderWriter > driver = osgDB::Registry::instance()->getReaderWriterForExtension(realExt);
        if (!driver.valid())
        {
            return ReadResult::FILE_NOT_HANDLED;
        }

        LoaderFile loader;
        Template t(loader);
        t.load( realName.c_str() );


        // Populate a list of key value pairs from the options string.
        if (options)
        {
            char escapeChar = '"';
            std::string key, value;
            bool currentCharIsKey = true;
            bool isComplexValue = false;
            std::string optionString = options->getOptionString();

            for(char& c : optionString)
            {
                //new value on the next char
                if(c == '=' && !isComplexValue)
                {
                    currentCharIsKey = false;
                }
                //Start or end of complex value on the next char
                else if(c == escapeChar)
                {
                    //we set the complex value
                    isComplexValue = !isComplexValue;
                    if(!isComplexValue)
                    {
                        if(!key.empty() && !value.empty())
                        {
                            t.set(key, value);
                            key = "";
                            value = "";
                        }
                    }
                }
                //new key on the next char
                else if(c == ' ' && !isComplexValue)
                {
                    currentCharIsKey = true;
                    if(!key.empty() && !value.empty())
                    {
                        t.set(key, value);
                        key = "";
                        value = "";
                    }
                }
                //normal character
                else
                {
                    if(isComplexValue || !currentCharIsKey)
                        value.push_back(c);
                    else
                        key.push_back(c);
                }
            }
        }

        OutputString output;
        t.render( output );

        OE_DEBUG << "Processed template " << std::endl << output.buf.str() << std::endl;

        // Set the osgEarth URIContext so that relative paths will work.  We have to do this manually here
        // since we are using the stream based readNode function and the Earth driver won't know
        // where the original earth file came frame.
        osg::ref_ptr< osgDB::Options > opt = osgEarth::Registry::instance()->cloneOrCreateOptions(options);
        osgEarth::URIContext( realName ).store( opt.get() );

        return driver->readNode( output.buf, opt.get() );
    }
};

REGISTER_OSGPLUGIN(template, TemplateReaderWriter)
