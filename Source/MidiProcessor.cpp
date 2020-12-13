#include "MidiProcessor.h"

MidiProcessor::MidiProcessor()
    : mappingNotes(12, std::vector<int>(1, 0))
{}

void MidiProcessor::registerListeners(AudioProcessorValueTreeState& treeState)
{
    initParameters();

    treeState.addParameterListener(ParamIDs::inChannel, &midiParams);
    treeState.addParameterListener(ParamIDs::outChannel, &midiParams);
    treeState.addParameterListener(ParamIDs::octaveTranspose, &midiParams);
    treeState.addParameterListener(ParamIDs::bypassChannels, &midiParams);

    for (auto& noteName : noteNames) {
        treeState.addParameterListener(noteName + ParamIDs::noteChoice, &noteParams);
        treeState.addParameterListener(noteName + ParamIDs::chordChoice, &noteParams);
    }
}

void MidiProcessor::process(MidiBuffer& midiMessages)
{
    MidiBuffer processedMidi;

    for (const auto metadata: midiMessages)
    {
        const auto m = metadata.getMessage();
        const auto matchingChannel = (m.getChannel() == inputChannel);
        // Only notes on and off from input channel are processed, the rest is passed through.
        if (matchingChannel && m.isNoteOnOrOff())
        {
            mapNote(m.getNoteNumber(), m.getVelocity(), m.isNoteOn(), metadata.samplePosition, processedMidi);
        }
        else 
        {
            if (!bypassOtherChannels || matchingChannel)
                processedMidi.addEvent(m, metadata.samplePosition);
        }
    }
    midiMessages.swapWith(processedMidi);
}

/*
    There are many cases to take in account here.
    We want the input to be monophonic so we need to know which was the last played note and 
    if there are some notes still on to be played when the last one is released.
*/
void MidiProcessor::mapNote(const int note, const juce::uint8 velocity, const bool noteOn, const int samplePosition, MidiBuffer& processedMidi)
{
    if (noteOn)
    {
        // Add the note to the vector of current notes played.
        currentInputNotesOn.push_back(note);

        // If the note changed, turn off the previous notes before adding the new ones.
        if (note != lastNoteOn && lastNoteOn > -1)
        {
            stopCurrentNotes(velocity, samplePosition, processedMidi);
        }
        // Play the received note.
        playMappedNotes(note, velocity, samplePosition, processedMidi);
    }
    else 
    {
        // For every note off, remove the received note from the vector of current notes held.
        removeHeldNote(note);
        
        // Turn off the corresponding notes for the current note off if it's the same as the last played note.
        // Otherwise it means the released note was not active so we don't need to do anything (case of multiple notes held)
        if (note == lastNoteOn) 
        {
            stopCurrentNotes(velocity, samplePosition, processedMidi);

            // If there were still some notes held, play the last one.
            if (currentInputNotesOn.size() > 0) {
                // Then play the last note from the vector of active input notes.
                playMappedNotes(currentInputNotesOn.back(), velocity, samplePosition, processedMidi);
            }
            else {
                // No note is currently played.
                lastNoteOn = -1;
            }
        }
    }
}

void MidiProcessor::playMappedNotes(const int note, const juce::uint8 velocity, const int samplePosition, MidiBuffer& processedMidi)
{
    lastNoteOn = note;
    // First clear the output notes vector to replace its values.
    currentOutputNotesOn.clear();

    const int baseNote = lastNoteOn % 12;
    for (int i = 0; i < mappingNotes[baseNote].size(); i++) {
        const int noteToPlay = lastNoteOn + mappingNotes[baseNote][i] + (i > 0 ? (octaveTranspose * 12) : 0);
        if (noteToPlay >= 0 && noteToPlay < 128) {
            processedMidi.addEvent(MidiMessage::noteOn(outputChannel, noteToPlay, velocity), samplePosition);
            currentOutputNotesOn.push_back(noteToPlay);
        }
    }
    currentNoteOutputChannel = outputChannel;
}

void MidiProcessor::stopCurrentNotes(const juce::uint8 velocity, const int samplePosition, MidiBuffer& processedMidi)
{
    for (const auto& currentNote: currentOutputNotesOn) {
        processedMidi.addEvent(MidiMessage::noteOff(currentNoteOutputChannel, currentNote, velocity), samplePosition);
    }
}

void MidiProcessor::removeHeldNote(const int note)
{
    for (auto it = currentInputNotesOn.begin(); it != currentInputNotesOn.end(); ++it)
    {
        if (*it == note)
        {
            currentInputNotesOn.erase(it);
            break;
        }
    }
}

void MidiProcessor::initParameters()
{
    updateMidiParams();
    
    for (auto& noteParam: noteParams.notes)
    {
        setMappedNotes(noteParam.noteIndex, noteParam.note->getIndex(), noteParam.chord->getIndex());
    }
}

void MidiProcessor::updateMidiParams()
{
    inputChannel = midiParams.inputChannel->get();
    outputChannel = midiParams.outputChannel->get();
    bypassOtherChannels = midiParams.bypassOtherChannels->get();
    octaveTranspose = midiParams.octaveTranspose->get();
}

void MidiProcessor::updateNoteParams(const String& paramID)
{
    for (auto& noteParam : noteParams.notes) {
        if (paramID == (noteParam.noteName + ParamIDs::noteChoice) || paramID == (noteParam.noteName + ParamIDs::chordChoice)) {
            setMappedNotes(noteParam.noteIndex, noteParam.note->getIndex(), noteParam.chord->getIndex());
            return;
        }
    }
}

void MidiProcessor::setMappedNotes(const int from_note, const int to_note, const int chord)
{
    // Declare a local vector for the mapping, initialized with the root note
    size_t chord_size = chordIntervals[chord].size();
    std::vector<int> mapping (chord_size, to_note - from_note);

    // If there's a chord, add its note + the octave transposition.
    if (chord_size > 1)
    {
        // Starting with 1 cause the first index is occupied by the root note
        for (int i = 1; i < chord_size; i++)
        {
            mapping[i] = to_note - from_note + chordIntervals[chord][i];
        }
    }
    // Replace the old mapping.
    mappingNotes[from_note].swap(mapping);
}

AudioProcessorValueTreeState::ParameterLayout MidiProcessor::getParameterLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    midiParams.addParams(layout);
    noteParams.addParams(layout);

    return layout;
}

int MidiProcessor::getLastNoteOn()
{
    return lastNoteOn;
}