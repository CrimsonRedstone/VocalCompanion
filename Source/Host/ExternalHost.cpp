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
    {lastError=error;return false;}
    if(desc.isInstrument){lastError="Select an audio effect, not an instrument.";return false;}
    // Disable optional sidechains and negotiate an ordinary stereo effect bus.
    inst->disableNonMainBuses();
    auto layout=inst->getBusesLayout();
    if(layout.inputBuses.isEmpty()||layout.outputBuses.isEmpty())
    {lastError="This plugin has no supported audio input/output buses.";return false;}
    layout.inputBuses.set(0,juce::AudioChannelSet::stereo());
    layout.outputBuses.set(0,juce::AudioChannelSet::stereo());
    if(!inst->setBusesLayout(layout))
    {lastError="This effect does not support stereo input/output.";return false;}
    lastError.clear();

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
    auto* ed = instance->createEditorAndMakeActive();
    // Parameter-only plugins still need a functional Full view.
    if(ed == nullptr) ed = new juce::GenericAudioProcessorEditor(*instance);
    if (ed != nullptr)
    {
        struct Window final : juce::DocumentWindow
        {using juce::DocumentWindow::DocumentWindow;void closeButtonPressed() override {setVisible(false);}};
        editorWindow = std::make_unique<Window> (
            instance->getName(), juce::Colours::black,
            juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton);
        editorWindow->setContentOwned (ed, true);
        editorWindow->setResizable (true, true);
        editorWindow->centreWithSize (ed->getWidth(), ed->getHeight());
        editorWindow->setVisible (true);
        editorWindow->setUsingNativeTitleBar (true);
    }
}

void ExternalModule::closeEditor()
{
    editorWindow.reset();
}

//==============================================================================
// VST3/AU do not expose the enclosing DAW's private plugin database. Keep a
// separate persistent catalog of installed formats this build can actually host.
void showPluginScannerDialog (std::function<void (juce::PluginDescription)> onChosen)
{
    class Browser final : public juce::Component, private juce::ChangeListener
    {
    public:
        explicit Browser(std::function<void(juce::PluginDescription)> choose):chosen(std::move(choose))
        {
            registerHostFormats(formats);
            folder=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("VocalCompanion");
            folder.createDirectory();
            if(auto xml=juce::XmlDocument::parse(folder.getChildFile("plugins.xml")))known.recreateFromXml(*xml);
            list=std::make_unique<juce::PluginListComponent>(formats,known,folder.getChildFile("scan-crash.txt"),nullptr);
            list->setOptionsButtonText("Scan / manage installed effects");
            list->setNumberOfThreadsForScanning(1);
            addAndMakeVisible(*list);addAndMakeVisible(info);addAndMakeVisible(load);
            info.setText("Scan VST3 (and AU on macOS), then select an effect. DAW-native effects, VST2 DLLs and CLAP hosting are not supported.",juce::dontSendNotification);
            load.setButtonText("Add selected effect");
            load.onClick=[this]{
                if(list->isScanning())return;
                auto types=known.getTypes();int row=list->getTableListBox().getSelectedRow();
                if(row<0||row>=types.size())return;
                auto desc=types[row];
                if(desc.isInstrument||desc.name.containsIgnoreCase("Vocal Companion"))return;
                auto cb=chosen;if(auto* window=findParentComponentOfClass<juce::DialogWindow>())window->exitModalState(0);
                if(cb)cb(desc);
            };
            known.addChangeListener(this);setSize(760,520);
        }
        ~Browser() override {known.removeChangeListener(this);if(auto xml=known.createXml())xml->writeTo(folder.getChildFile("plugins.xml"));}
        void resized() override {auto r=getLocalBounds().reduced(12);info.setBounds(r.removeFromTop(44));load.setBounds(r.removeFromBottom(30));r.removeFromBottom(8);list->setBounds(r);}
    private:
        void changeListenerCallback(juce::ChangeBroadcaster*) override
        {
            // Prevent instruments/recursive copies from being offered as effects.
            for(auto d:known.getTypes())if(d.isInstrument||d.name.containsIgnoreCase("Vocal Companion"))known.removeType(d);
            if(auto xml=known.createXml())xml->writeTo(folder.getChildFile("plugins.xml"));
        }
        juce::AudioPluginFormatManager formats;juce::KnownPluginList known;juce::File folder;
        std::unique_ptr<juce::PluginListComponent> list;
        juce::Label info;juce::TextButton load;std::function<void(juce::PluginDescription)> chosen;
    };
    juce::DialogWindow::LaunchOptions options;options.content.setOwned(new Browser(std::move(onChosen)));
    options.dialogTitle="Installed external effects";options.dialogBackgroundColour=juce::Colour(0xff161c24);
    options.useNativeTitleBar=true;options.resizable=true;options.launchAsync();
}

} // namespace vc
