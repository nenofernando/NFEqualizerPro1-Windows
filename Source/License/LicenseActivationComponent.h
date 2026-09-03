#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "NFLicenseManager.h"

/** Popup de ativação centralizado - fundo semi-transparente cobre a janela
    toda (dá pra ver o plugin por trás, mas fica claro que precisa ativar),
    com um cartão bem visível no meio (campo de chave + botão + status).
    Feedback do usuário: uma barra fina no topo passava despercebida - isso
    aqui é bem mais difícil de ignorar. Some sozinho quando a ativação der
    certo. */
class LicenseActivationComponent : public juce::Component
{
public:
    explicit LicenseActivationComponent (NFLicenseManager& managerIn) : manager (managerIn)
    {
        titleLabel.setText ("Ative sua licenca", juce::dontSendNotification);
        titleLabel.setJustificationType (juce::Justification::centred);
        titleLabel.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
        titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (titleLabel);

        keyEditor.setTextToShowWhenEmpty ("XXXXX-XXXXX-XXXXX-XXXXX", juce::Colours::grey);
        keyEditor.setJustification (juce::Justification::centred);
        keyEditor.setFont (juce::Font (juce::FontOptions (16.0f)));
        addAndMakeVisible (keyEditor);

        activateButton.setButtonText ("Activate License");
        activateButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff7c3aed));
        activateButton.onClick = [this] { attemptActivation(); };
        addAndMakeVisible (activateButton);

        statusLabel.setJustificationType (juce::Justification::centred);
        statusLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
        addAndMakeVisible (statusLabel);
    }

    void paint (juce::Graphics& g) override
    {
        // Fundo escurecido semi-transparente - o plugin continua visível
        // por baixo, só fica claro que a interação está bloqueada.
        g.fillAll (juce::Colour (0x70000000));

        auto card = cardBounds();
        g.setColour (juce::Colour (0xf2141416));
        g.fillRoundedRectangle (card.toFloat(), 14.0f);
        g.setColour (juce::Colour (0xff7c3aed));
        g.drawRoundedRectangle (card.toFloat(), 14.0f, 1.5f);
    }

    void resized() override
    {
        auto area = cardBounds().reduced (28, 24);
        titleLabel.setBounds (area.removeFromTop (32));
        area.removeFromTop (16);
        keyEditor.setBounds (area.removeFromTop (36));
        area.removeFromTop (14);
        activateButton.setBounds (area.removeFromTop (38));
        area.removeFromTop (12);
        statusLabel.setBounds (area.removeFromTop (24));
    }

private:
    juce::Rectangle<int> cardBounds() const
    {
        return getLocalBounds().withSizeKeepingCentre (360, 220);
    }

    void attemptActivation()
    {
        auto key = keyEditor.getText().trim();
        if (key.isEmpty())
        {
            statusLabel.setText ("Digite a chave de licenca.", juce::dontSendNotification);
            statusLabel.setColour (juce::Label::textColourId, juce::Colours::orange);
            return;
        }

        activateButton.setEnabled (false);
        statusLabel.setColour (juce::Label::textColourId, juce::Colours::white);
        statusLabel.setText ("Verificando...", juce::dontSendNotification);

        manager.activateAsync (key, [this] (bool success, juce::String errorMessage)
        {
            activateButton.setEnabled (true);

            if (success)
            {
                statusLabel.setColour (juce::Label::textColourId, juce::Colours::limegreen);
                statusLabel.setText ("Ativado!", juce::dontSendNotification);
                if (onActivated)
                    onActivated();
            }
            else
            {
                statusLabel.setColour (juce::Label::textColourId, juce::Colours::orangered);
                statusLabel.setText (errorMessage, juce::dontSendNotification);
            }
        });
    }

public:
    std::function<void()> onActivated;

private:
    NFLicenseManager& manager;
    juce::Label titleLabel, statusLabel;
    juce::TextEditor keyEditor;
    juce::TextButton activateButton;
};
