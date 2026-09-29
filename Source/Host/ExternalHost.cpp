#include "Host/ExternalHost.h"

namespace vc
{

static void registerHostFormats (juce::AudioPluginFormatManager& fm)
{
    // JUCE 9 deleted AudioPluginFormatManager::addDefaultFormats().
    juce::addDefaultFormatsToManager (fm);
}

ExternalModule::ExternalModule()
{
    registerHostFormats (formatManager);
}

ExternalModule::~ExternalModule()
{
    closeEditor();
    instance.reset();
}

juce::String ExternalModule::getDisplayName() const
{
    if (instance != nullptr)
        return instance->getName();
    return "EXTERNAL VST";
}

void ExternalModule::prepare (double sr, int block, int chs)
{
    sampleRate = sr;
    blockSize = block;
    numCh = chs;
    if (instance != nullptr)
    {
        instance->setRateAndBufferSizeDetails (sr, block);
        instance->prepareToPlay (sr, block);
    }
}

void ExternalModule::reset()
{
    if (instance != nullptr)
        instance->reset();
}

void ExternalModule::process (juce::AudioBuffer<float>& buffer)
{
    if (instance == nullptr)
        return;
    emptyMidi.clear();
    instance->processBlock (buffer, emptyMidi);
}

bool ExternalModule::loadFromFile (const juce::File& file)
{
    juce::OwnedArray<juce::PluginDescription> types;
    for (int i = 0; i < formatManager.getNumFormats(); ++i)
        formatManager.getFormat (i)->findAllTypesForFile (types, file.getFullPathName());

    if (types.isEmpty())
        return false;
    return loadFromDescription (*types[0]);
}

bool ExternalModule::loadFromDescription (const juce::PluginDescription& desc)
{
    closeEditor();
    juce::String error;
    auto inst = formatManager.createPluginInstance (desc, sampleRate, blockSize, error);
    if (inst == nullptr)
        return false;

    description = desc;
    instance = std::move (inst);
    instance->setRateAndBufferSizeDetails (sampleRate, blockSize);
    instance->prepareToPlay (sampleRate, blockSize);
    if (pluginState.getSize() > 0)
        instance->setStateInformation (pluginState.getData(), (int) pluginState.getSize());
    return true;
}

void ExternalModule::closePlugin()
{
    closeEditor();
    instance.reset();
    pluginState.reset();
}

juce::ValueTree ExternalModule::toValueTree() const
{
    auto t = VcModule::toValueTree();
    t.setProperty ("pluginXml", description.createXml() != nullptr
                                    ? description.createXml()->toString()
                                    : juce::String(), nullptr);
    if (instance != nullptr)
    {
        juce::MemoryBlock mb;
        instance->getStateInformation (mb);
        t.setProperty ("pluginState", mb.toBase64Encoding(), nullptr);
    }
    return t;
}

void ExternalModule::fromValueTree (const juce::ValueTree& t)
{
    VcModule::fromValueTree (t);
    const auto xmlStr = t.getProperty ("pluginXml").toString();
    if (xmlStr.isNotEmpty())
    {
        if (auto xml = juce::XmlDocument::parse (xmlStr))
        {
            juce::PluginDescription restored;restored.loadFromXml(*xml);
            const bool same=instance!=nullptr&&description.createIdentifierString()==restored.createIdentifierString();
            description=restored;
            const auto st = t.getProperty ("pluginState").toString();
            pluginState.reset();
            if (st.isNotEmpty())
                pluginState.fromBase64Encoding (st);
            if(same)instance->setStateInformation(pluginState.getData(),(int)pluginState.getSize());
            else loadFromDescription (description);
        }
    }
}

void ExternalModule::openEditor()
{
    if (instance == nullptr)
        return;
    closeEditor();
    if (auto* ed = instance->createEditorAndMakeActive())
    {
        editorWindow = std::make_unique<juce::DocumentWindow> (
            instance->getName(), juce::Colours::black,
            juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton);
        editorWindow->setContentNonOwned (ed, true);
        editorWindow->setResizable (true, true);
        editorWindow->centreWithSize (ed->getWidth(), ed->getHeight());
        editorWindow->setVisible (true);
        editorWindow->setUsingNativeTitleBar (true);
    }
}

void ExternalModule::closeEditor()
{
    if (instance != nullptr)
        instance->editorBeingDeleted (instance->getActiveEditor());
    editorWindow.reset();
}

//==============================================================================
void showPluginScannerDialog (std::function<void (juce::PluginDescription)> onChosen)
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Select a VST3 / AU / CLAP plugin",
        juce::File(),
        "*.vst3;*.component;*.clap;*.dll");

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectDirectories,
                          [chooser, onChosen] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (! f.exists())
                                  return;
                              juce::AudioPluginFormatManager fm;
                              registerHostFormats (fm);
                              juce::OwnedArray<juce::PluginDescription> types;
                              for (int i = 0; i < fm.getNumFormats(); ++i)
                                  fm.getFormat (i)->findAllTypesForFile (types, f.getFullPathName());
                              if (types.size() > 0 && onChosen)
                                  onChosen (*types[0]);
                          });
}

} // namespace vc
