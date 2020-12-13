#pragma once

#include "JuceHeader.h"
#include "ParameterList.h"

namespace ParamIDs
{
    static String inChannel{ "in_channel" };
    static String outChannel{ "out_channel" };
    static String octaveTranspose{ "octave_transpose" };
    static String bypassChannels{ "bypass_other_channels" };
    static String noteChoice{ "_note" };
    static String chordChoice{ "_chord" };
}

using Layout = AudioProcessorValueTreeState::ParameterLayout;

/*
    Simple structure that contains the 4 basic parameters at the top of the plugin.
    Whenever one of them is updated, the parameterChanged method will update the local atomic values for all of them.
*/
struct MidiParams : AudioProcessorValueTreeState::Listener
{
    MidiParams(std::function<void()> lambda)
        : update(lambda)
    {}

    void addParams(Layout& layout)
    {
        ParameterList list(layout);

        inputChannel = list.add<AudioParameterInt>(ParamIDs::inChannel, "Input Channel", 1, 16, 1, "Input Channel");
        outputChannel = list.add<AudioParameterInt>(ParamIDs::outChannel, "Output Channel", 1, 16, 1, "Output Channel");
        octaveTranspose = list.add<AudioParameterInt>(ParamIDs::octaveTranspose, "Transpose octaves", -1, 4, 0, "Transpose octaves");
        bypassOtherChannels = list.add<AudioParameterBool>(ParamIDs::bypassChannels, "Bypass other channels", false, "Bypass other channels");
    }

    void parameterChanged(const String& paramID, float newValue) {
        if (update != nullptr) update();
    }

    std::function<void()> update = nullptr;

    AudioParameterInt* inputChannel = nullptr;
    AudioParameterInt* outputChannel = nullptr;
    AudioParameterInt* octaveTranspose = nullptr;
    AudioParameterBool* bypassOtherChannels = nullptr;
};

/*
    This represents the structure for a note/chord couple parameters.
*/
struct NoteParam 
{
    NoteParam(const int i, const String n)
        : noteIndex(i), noteName(n)
    {}

    void addParams(Layout& layout, const StringArray& noteNames, const StringArray& chordNames)
    {
        ParameterList list(layout);
        note = list.add<AudioParameterChoice>(noteName + ParamIDs::noteChoice, noteName, noteNames, noteIndex, noteName);
        chord = list.add<AudioParameterChoice>(noteName + ParamIDs::chordChoice, noteName + " chord", chordNames, 0, noteName + " chord");
    }

    int noteIndex;
    String noteName;
    AudioParameterChoice* note = nullptr;
    AudioParameterChoice* chord = nullptr;
};

/*
    This structure contains all 12 note/chord couples. A listener is attached so we can know which mapping has to be updated
    when one of the parameters is changed.
*/
struct NoteParams : AudioProcessorValueTreeState::Listener
{
    NoteParams(const StringArray& noteNames, const StringArray& chordNames, std::function<void(const String& paramID)> lambda = nullptr)
        : noteNames(noteNames), chordNames(chordNames), update(lambda)
    {
        for (int i = 0; i < noteNames.size(); i++) {
            notes.emplace_back(i, noteNames[i]);
        }
    }

    void addParams(Layout& layout)
    {
        for (auto& note : notes) {
            note.addParams(layout, noteNames, chordNames);
        }
    }

    void parameterChanged(const String& paramID, float newValue) {
        if (update != nullptr) update(paramID);
    }

    StringArray noteNames;
    StringArray chordNames;
    std::vector<NoteParam> notes;

    std::function<void(const String& paramID)> update = nullptr;
};
