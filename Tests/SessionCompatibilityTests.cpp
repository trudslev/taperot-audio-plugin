#include "TestUtils.h"
#include "../Source/DSP/FactoryPrograms.h"
#include "../Source/Parameters.h"

#include <set>
#include <juce_audio_processors/juce_audio_processors.h>

/**
    Guards saved-session and saved-Program compatibility across the Preset -> Program rename.

    The rename touched a lot of identifiers - FactoryPreset, kFactoryPresets, isFactoryPreset,
    saveUserPreset, the on-disk folder and the file extension - but deliberately touched none of the
    strings that key saved state. Those are the parameter IDs, the schema-version attribute and
    `taperotCurrentProgramIndex`. Rename one of those and every affected value silently reverts to
    its default on load, with no error anywhere: the XML still parses, the APVTS just never finds a
    parameter matching the id, and the user's session comes back subtly wrong.

    So the fixture below spells every id as a **literal**, not as `ParamIDs::drive`. Written through
    ParamIDs it would rename itself alongside the code and assert nothing. Written as literals it is
    a fixed record of the on-disk contract, and renaming an id breaks this test instead of a
    session.

    The XML is a verbatim copy of a real file written by the shipping build (a saved User Program
    from 2026-07-10, values and float formatting untouched), so this exercises the actual format
    rather than one reconstructed from the writer.
*/
class SessionCompatibilityTests final : public juce::UnitTest
{
public:
    SessionCompatibilityTests() : juce::UnitTest("Session compatibility", "State") {}

    // Verbatim, including the trailing "stopAutoEngage" entry - a parameter this build no longer
    // declares. An unknown id must be ignored rather than rejected, or a session saved by a newer
    // build would fail to load in an older one.
    static constexpr const char* shippingBuildProgram = R"(<?xml version="1.0" encoding="UTF-8"?>

<PARAMETERS taperotStateSchemaVersion="3" taperotCurrentProgramIndex="1">
  <PARAM id="drive" value="35.00000381469727"/>
  <PARAM id="failure" value="0.0"/>
  <PARAM id="failureCrinkles" value="1.0"/>
  <PARAM id="failureDropouts" value="1.0"/>
  <PARAM id="failureImbalance" value="1.0"/>
  <PARAM id="failureSnags" value="1.0"/>
  <PARAM id="flutter" value="3.237889289855957"/>
  <PARAM id="gen" value="2.0"/>
  <PARAM id="hp" value="20.0"/>
  <PARAM id="hum" value="0.0"/>
  <PARAM id="lp" value="20000.0"/>
  <PARAM id="mix" value="100.0"/>
  <PARAM id="model" value="6.0"/>
  <PARAM id="noise" value="25.0"/>
  <PARAM id="noiseCharacter" value="0.0"/>
  <PARAM id="output" value="0.0"/>
  <PARAM id="ramp" value="0.2999999821186066"/>
  <PARAM id="spread" value="0.0"/>
  <PARAM id="switchMode" value="0.0"/>
  <PARAM id="wow" value="30.00000190734863"/>
  <PARAM id="stopAutoEngage" value="1.0"/>
</PARAMETERS>)";

    class DummyProcessor final : public juce::AudioProcessor
    {
    public:
        const juce::String getName() const override { return "Dummy"; }
        void prepareToPlay(double, int) override {}
        void releaseResources() override {}
        void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        double getTailLengthSeconds() const override { return 0.0; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram(int) override {}
        const juce::String getProgramName(int) override { return {}; }
        void changeProgramName(int, const juce::String&) override {}
        void getStateInformation(juce::MemoryBlock&) override {}
        void setStateInformation(const void*, int) override {}
    };

    void runTest() override
    {
        beginTest("A Program saved by the shipping build still restores every value");
        {
            DummyProcessor proc;
            juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMETERS",
                                                     createTapeRotParameterLayout());

            std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(shippingBuildProgram));
            expect(xml != nullptr, "the fixture must parse");
            expect(xml->hasTagName(apvts.state.getType()),
                   "the root tag must still be the one the APVTS was built with");

            LegacyMigration::remapLegacyModelIndexIfNeeded(*xml);
            apvts.replaceState(juce::ValueTree::fromXml(*xml));

            // Ids as literals, on purpose - see the class comment.
            const auto get = [&] (const char* id)
            {
                auto* v = apvts.getRawParameterValue(id);
                expect(v != nullptr, juce::String("no parameter matches saved id \"") + id + "\"");
                return v != nullptr ? v->load() : 0.0f;
            };

            expectWithinAbsoluteError(get("drive"),   35.0f,  0.001f);
            expectWithinAbsoluteError(get("wow"),     30.0f,  0.001f);
            expectWithinAbsoluteError(get("flutter"),  3.2379f, 0.001f);
            expectWithinAbsoluteError(get("noise"),   25.0f,  0.001f);
            expectWithinAbsoluteError(get("mix"),    100.0f,  0.001f);
            expectWithinAbsoluteError(get("output"),   0.0f,  0.001f);
            expectWithinAbsoluteError(get("lp"),   20000.0f,  0.001f);
            expectWithinAbsoluteError(get("hp"),      20.0f,  0.001f);
            expectWithinAbsoluteError(get("ramp"),     0.3f,  0.001f);
            expectEquals((int) get("gen"), 2);
            expectEquals((int) get("noiseCharacter"), 0);

            // Schema 3 is current, so the model index must survive untouched rather than being
            // remapped a second time. 6 is CASSETTE II.
            expectEquals((int) get("model"), 6);

            expect(get("hum") < 0.5f);
            expect(get("spread") < 0.5f);
            expect(get("switchMode") < 0.5f);
            expect(get("failureCrinkles") > 0.5f);
            expect(get("failureDropouts") > 0.5f);
            expect(get("failureImbalance") > 0.5f);
            expect(get("failureSnags") > 0.5f);
        }

        beginTest("The remembered program index survives under its original attribute name");
        {
            std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(shippingBuildProgram));
            expect(xml != nullptr);

            // Spelled out rather than taken from a constant: this attribute is written into every
            // saved session, so renaming it would quietly reset everyone's remembered Program.
            expectEquals(xml->getIntAttribute("taperotCurrentProgramIndex", -1), 1,
                         "the saved program index must still be found under this exact name");
            expectEquals(xml->getIntAttribute("taperotStateSchemaVersion", -1), 3,
                         "the schema-version attribute must still be found under this exact name");
        }

        beginTest("The identity attributes are spelled exactly as the shipping format has them");
        {
            // **Literals, not the constants.** Through LegacyMigration:: these would rename
            // themselves alongside the code and assert nothing; the whole point is that a rename
            // breaks this test rather than everyone's sessions.
            expect(juce::String(LegacyMigration::programBankAttribute) == "taperotProgramBank");
            expect(juce::String(LegacyMigration::programIdAttribute)   == "taperotProgramId");
            expect(juce::String(LegacyMigration::programNameAttribute) == "taperotProgramName");
            expect(juce::String(LegacyMigration::stateSchemaVersionAttribute) == "taperotStateSchemaVersion");
        }

        beginTest("Bank values round-trip through their attribute spelling");
        {
            for (const auto bank : { ProgramBank::init, ProgramBank::factory,
                                     ProgramBank::user, ProgramBank::unresolved })
                expect(LegacyMigration::bankFromAttribute(
                           LegacyMigration::bankAttributeValue(bank)) == bank);

            // An unreadable or absent bank must not silently become INIT or unresolved - the
            // factory bank is the only safe default, because it is the one that always exists.
            expect(LegacyMigration::bankFromAttribute("") == ProgramBank::factory);
            expect(LegacyMigration::bankFromAttribute("nonsense") == ProgramBank::factory);
        }

        beginTest("Factory slugs are unique, non-empty, and pinned as literals");
        {
            std::set<juce::String> slugs;

            for (const auto& fp : kFactoryPrograms)
            {
                const juce::String slug { fp.slug };
                expect(slug.isNotEmpty(), juce::String(fp.name) + " has no slug");
                expect(slugs.insert(slug).second, "duplicate slug: " + slug);
                expect(! slug.containsChar(' '), "slug must be filename- and XML-safe: " + slug);
            }

            expectEquals((int) slugs.size(), (int) kFactoryPrograms.size());

            // **Literals on purpose.** A slug is the permanent identity: the display name above it
            // may be revised freely, but changing one of these orphans every session that stored
            // it. Asserted through kFactoryPrograms[n].slug this would follow the rename silently.
            expect(juce::String(kFactoryPrograms[0].slug) == "warm-cassette");
            expect(juce::String(kFactoryPrograms[12].slug) == "total-meltdown");
            expect(juce::String(kInitProgram.slug) == "init");
        }

        beginTest("The schema version is 5, and the bump is the move to identity - nothing else");
        // The reasoning this guard was written for still stands: a bump must be justified by an
        // actual format change, not by a rename. It has now been justified twice - v4 when Init
        // left the numbered bank and every index shifted, and v5 when the session stopped storing
        // a position at all and started storing bank + identifier.
        expectEquals(LegacyMigration::currentStateSchemaVersion, 5,
                     "only a real state-format change may bump this");

        beginTest("A v3 session's program index is remapped across the Init promotion");
        {
            // The fixture above is a REAL file written by the shipping v3 build, and it remembers
            // index 1 - which under the old numbering was Warm Cassette, sitting behind Init at 0.
            // Under v4 that same sound is index 0. Getting this wrong is silent: the knob values
            // restore correctly and only the NAME on the panel is wrong, so it names a sound it is
            // not making.
            std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(shippingBuildProgram));
            expect(xml != nullptr);

            const int saved = xml->getIntAttribute("taperotCurrentProgramIndex", -99);
            expectEquals(saved, 1, "the fixture should be the v3 build's Warm Cassette");
            expectEquals(LegacyMigration::remapProgramIndexV3ToV4(saved), 0,
                         "v3's Warm Cassette (1) must become v4's Warm Cassette (0)");

            // Old index 0 was Init itself, which is no longer in the bank at all.
            expectEquals(LegacyMigration::remapProgramIndexV3ToV4(0), initProgramIndex,
                         "v3's Init (0) must become the unnumbered INIT slot");

            // The shift is uniform, so User Programs ride along on the same subtraction.
            expectEquals(LegacyMigration::remapProgramIndexV3ToV4(13), 12);
        }

        beginTest("INIT is outside both banks and is not the instantiation default");
        {
            expect(initProgramIndex < 0, "INIT must not occupy a bank index");
            expectEquals((int) defaultFactoryProgramIndex, 0,
                         "the instantiation default must be Program 01, not the entry behind it");
            expectEquals((int) kNumFactoryPrograms, 13,
                         "Init left the numbered bank, so thirteen authored Programs remain");
            expect(juce::String(kFactoryPrograms[defaultFactoryProgramIndex].name) == "WARM CASSETTE");
        }

        beginTest("An unknown parameter id in a saved session is ignored, not fatal");
        {
            // "stopAutoEngage" in the fixture no longer exists. Loading must still succeed and
            // leave every known parameter correct - already asserted above, so this just pins the
            // intent: forward compatibility is a property of the format, not an accident.
            DummyProcessor proc;
            juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMETERS",
                                                     createTapeRotParameterLayout());
            expect(apvts.getParameter("stopAutoEngage") == nullptr,
                   "the fixture's obsolete id must genuinely be absent from this build");

            std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(shippingBuildProgram));
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
            expectWithinAbsoluteError(apvts.getRawParameterValue("drive")->load(), 35.0f, 0.001f,
                                      "values after the unknown id must still restore");
        }
    }
};

static SessionCompatibilityTests sessionCompatibilityTests;
