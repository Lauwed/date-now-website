<script lang="ts">
	import { PUBLIC_API_URL } from '$env/static/public';
	import Button from './Button.svelte';
	import FormControl from './Form/FormControl.svelte';
	import Modal from './Modal.svelte';
	import Spinner from './Spinner.svelte';

	let state = $state<'form' | 'loading' | 'success' | 'error'>('form');
	let email = $state('');
	let errorText = $state('Something wrong happened... Try again later please');

	const handleSubmit = async (
		event: SubmitEvent & { currentTarget: EventTarget & HTMLFormElement }
	) => {
		event.preventDefault();

		state = 'loading';

		const res = await fetch(`${PUBLIC_API_URL}/api/auth/subscribe`, {
			method: 'POST',
			body: JSON.stringify({ email })
		});
		console.log(res);
		if (res.ok) {
			const json = await res.json();
			console.log(json);

			state = 'success';
		} else {
			if (res.status === 409) {
				errorText =
					"You are already subscribed 🥸 I know you can't wait to receive the next issue. I might be in a new Minecraft phase and the newsletter is late...";
			} else {
				errorText = 'Something went wrong. Try again later or contact us.';
			}

			console.error(`Error while sending confirmation mail. Code: ${res.status}`);
			state = 'error';
		}
	};
</script>

<Modal id="subscribe-modal" customClass="subscribe" title="Subscribe to the newsletter">
	{#if state === 'form'}
		<div class="subscribe__info">
			<p>Thanks for considering to subscribe to my newsletters, means a lot 🥹.</p>
			<p>
				I encourage you to read the <a target="_blank" href="/legal-notice">Legal Notice</a> to know more
				about how your data is treated.
			</p>
		</div>

		<form class="subscribe__form" onsubmit={handleSubmit}>
			<FormControl id="email" label="Email" required type="email" bind:value={email} />

			<Button variant="primary" type="submit">Submit</Button>
		</form>
	{:else if state === 'loading'}
		<Spinner text="Sending the subscription confirmation mail..." />
	{:else if state === 'error'}
		<div class="subscribe__result">
			<p>Error...</p>
			<p>{errorText}</p>
		</div>
	{:else if state === 'success'}
		<div class="subscribe__result">
			<p>Almost done!</p>
			<p>
				Please, go check your email box and confirm your subscription to the newsletter to receive
				it.
			</p>
		</div>
	{/if}
</Modal>

<style lang="scss">
	:global(.subscribe .modal__content) {
		display: flex;
		flex-direction: column;
		gap: 12px;
	}

	.subscribe {
		&__info {
			p {
				margin: 0;
			}
		}
		&__form {
			width: 100%;
			display: flex;
			flex-direction: column;
			gap: 12px;
			align-items: end;
		}
		&__result {
			p {
				margin: 0;
				&:first-child {
					font-size: 1.5rem;
				}
			}
		}
	}
</style>
